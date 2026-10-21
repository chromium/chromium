// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_tab_helper.h"

#include "base/files/file_path.h"
#include "base/time/time.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/web_applications/web_app_launch_navigation_handle_user_data.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/webapps/browser/launch_queue/launch_params.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {
namespace {

class WebAppTabHelperTest : public WebAppTest {
 public:
  void SetUp() override {
    WebAppTest::SetUp();
    test::AwaitStartWebAppProviderAndSubsystems(profile());

    ON_CALL(browser_window_interface_, GetProfile())
        .WillByDefault(testing::Return(profile()));
    ON_CALL(tab_interface_, GetBrowserWindowInterface())
        .WillByDefault(testing::Return(&browser_window_interface_));
    ON_CALL(tab_interface_, GetContents())
        .WillByDefault(testing::Return(web_contents()));

    WebAppTabHelper::Create(&tab_interface_, web_contents());
  }

  WebAppTabHelper* tab_helper() {
    return WebAppTabHelper::FromWebContents(web_contents());
  }

 private:
  testing::NiceMock<MockBrowserWindowInterface> browser_window_interface_;
  tabs::MockTabInterface tab_interface_;
};

// Regression test for https://crbug.com/574231826: if a navigation reaches
// `ReadyToCommitNavigation` (updating `app_id_` to the target app) and then
// finishes without committing, `DidFinishNavigation` must restore `app_id_` to
// match the tab's last committed URL, and `CanBeUsedForFocusExisting()` must
// never return true while the committed URL is outside the app's extended
// scope.
TEST_F(WebAppTabHelperTest, AbortedNavigationRestoresCommittedAppId) {
  const webapps::AppId app_id = test::InstallDummyWebApp(
      profile(), "Target App", GURL("https://app.example/scope/"));

  const GURL unrelated_url("https://unrelated.example/page.html");
  auto initial_nav = content::NavigationSimulator::CreateBrowserInitiated(
      unrelated_url, web_contents());
  initial_nav->SetContentsMimeType("text/html");
  initial_nav->Commit();

  EXPECT_EQ(nullptr, WebAppTabHelper::GetAppId(web_contents()));
  EXPECT_FALSE(tab_helper()->CanBeUsedForFocusExisting());

  // Simulate a primary main-frame navigation to the target app that reaches
  // `ReadyToCommitNavigation` and then aborts before committing.
  content::MockNavigationHandle mock_nav_handle(
      GURL("https://app.example/scope/launch"), main_rfh());
  mock_nav_handle.set_is_in_primary_main_frame(true);
  mock_nav_handle.set_is_same_document(false);
  mock_nav_handle.set_has_committed(false);

  tab_helper()->ReadyToCommitNavigation(&mock_nav_handle);
  ASSERT_NE(nullptr, WebAppTabHelper::GetAppId(web_contents()));
  EXPECT_EQ(app_id, *WebAppTabHelper::GetAppId(web_contents()));
  // Even while `app_id_` is provisionally set before commit, the committed URL
  // (`unrelated_url`) is not in scope of `app_id`, so in-place focus-existing
  // reuse must be rejected.
  EXPECT_FALSE(tab_helper()->CanBeUsedForFocusExisting());

  // When the navigation finishes without committing, `app_id_` is reconciled
  // back to the committed URL (`std::nullopt`).
  tab_helper()->DidFinishNavigation(&mock_nav_handle);
  EXPECT_EQ(nullptr, WebAppTabHelper::GetAppId(web_contents()));
  EXPECT_FALSE(tab_helper()->CanBeUsedForFocusExisting());
}

// Verifies that normal cross-document committed navigations within an app's
// scope keep `app_id_` set to that app, and that same-document navigations
// (`IsSameDocument()`) do not flip `app_id_` in `DidFinishNavigation` while
// `CanBeUsedForFocusExisting()` still enforces `GetLastCommittedURL()` scope.
TEST_F(WebAppTabHelperTest,
       CommittedAndSameDocumentNavigationsPreserveExpectedAppId) {
  const webapps::AppId app_id = test::InstallDummyWebApp(
      profile(), "App", GURL("https://app.example/scope/"));

  auto nav1 = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://app.example/scope/page1.html"), web_contents());
  nav1->SetContentsMimeType("text/html");
  nav1->Commit();
  ASSERT_NE(nullptr, WebAppTabHelper::GetAppId(web_contents()));
  EXPECT_EQ(app_id, *WebAppTabHelper::GetAppId(web_contents()));
  EXPECT_TRUE(tab_helper()->CanBeUsedForFocusExisting());

  // A second cross-document navigation within the same app's scope keeps
  // `app_id_` unchanged.
  auto nav2 = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://app.example/scope/page2.html"), web_contents());
  nav2->SetContentsMimeType("text/html");
  nav2->Commit();
  ASSERT_NE(nullptr, WebAppTabHelper::GetAppId(web_contents()));
  EXPECT_EQ(app_id, *WebAppTabHelper::GetAppId(web_contents()));
  EXPECT_TRUE(tab_helper()->CanBeUsedForFocusExisting());

  // A same-document navigation (`pushState`) to an out-of-scope path on the
  // same origin does not flip `app_id_` in `DidFinishNavigation`, but
  // `CanBeUsedForFocusExisting()` rejects it because `GetLastCommittedURL()`
  // is no longer in extended scope of `app_id_`.
  auto same_doc_nav = content::NavigationSimulator::CreateRendererInitiated(
      GURL("https://app.example/out-of-scope/page.html"), main_rfh());
  same_doc_nav->CommitSameDocument();
  ASSERT_NE(nullptr, WebAppTabHelper::GetAppId(web_contents()));
  EXPECT_EQ(app_id, *WebAppTabHelper::GetAppId(web_contents()));
  EXPECT_FALSE(tab_helper()->CanBeUsedForFocusExisting());
}

// Regression test for https://crbug.com/574231826:
// `CanBeUsedForFocusExisting()` must verify that `GetLastCommittedURL()` is in
// the extended scope of `app_id_`.
TEST_F(WebAppTabHelperTest,
       CanBeUsedForFocusExistingRequiresCommittedUrlInAppScope) {
  const webapps::AppId app_a = test::InstallDummyWebApp(
      profile(), "App A", GURL("https://app-a.example/app/"));
  const webapps::AppId app_b = test::InstallDummyWebApp(
      profile(), "App B", GURL("https://app-b.example/app/"));

  auto nav = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://app-a.example/app/index.html"), web_contents());
  nav->SetContentsMimeType("text/html");
  nav->Commit();

  ASSERT_NE(nullptr, WebAppTabHelper::GetAppId(web_contents()));
  EXPECT_EQ(app_a, *WebAppTabHelper::GetAppId(web_contents()));
  EXPECT_TRUE(tab_helper()->CanBeUsedForFocusExisting());

  // If `app_id_` does not match the committed URL's scope (e.g. set to `app_b`
  // while `app_a`'s URL is committed), `CanBeUsedForFocusExisting()` must
  // return false.
  tab_helper()->SetAppId(app_b);
  EXPECT_FALSE(tab_helper()->CanBeUsedForFocusExisting());
}

// Regression test for https://crbug.com/574232164: `SetLaunchParamsMetadata`
// must preserve `paths()` and `dir()` across same-app redirects, and clear both
// when a redirect changes the target `app_id`.
TEST_F(WebAppTabHelperTest,
       SetLaunchParamsMetadataClearsFilesAndDirOnCrossAppRedirect) {
  content::MockNavigationHandle mock_nav_handle(
      GURL("https://app-a.example/start"), main_rfh());
  mock_nav_handle.set_is_in_primary_main_frame(true);

  auto* user_data =
      WebAppLaunchNavigationHandleUserData::GetOrCreateForNavigationHandle(
          mock_nav_handle);

  webapps::LaunchParams initial_params;
  initial_params.set_app_id("app_a");
  initial_params.set_target_url(GURL("https://app-a.example/start"));
  initial_params.set_paths_with_permissions(
      {base::FilePath(FILE_PATH_LITERAL("user_doc.txt"))}, {true});
  initial_params.set_dir(base::FilePath(FILE_PATH_LITERAL("user_dir")));
  user_data->SetLaunchParams(std::move(initial_params));

  // Same-app redirect updates `target_url` while keeping file/directory
  // capabilities intact.
  user_data->SetLaunchParamsMetadata("app_a",
                                     GURL("https://app-a.example/redirected"),
                                     base::TimeTicks::Now());
  EXPECT_EQ("app_a", user_data->GetLaunchParams().app_id());
  EXPECT_EQ(GURL("https://app-a.example/redirected"),
            user_data->GetLaunchParams().target_url());
  EXPECT_EQ(1u, user_data->GetLaunchParams().paths().size());
  EXPECT_EQ(1u, user_data->GetLaunchParams().can_write().size());
  EXPECT_FALSE(user_data->GetLaunchParams().dir().empty());

  // Cross-app redirect to `app_b` strips `paths`, `can_write`, and `dir`.
  user_data->SetLaunchParamsMetadata(
      "app_b", GURL("https://app-b.example/landing"), base::TimeTicks::Now());
  EXPECT_EQ("app_b", user_data->GetLaunchParams().app_id());
  EXPECT_EQ(GURL("https://app-b.example/landing"),
            user_data->GetLaunchParams().target_url());
  EXPECT_TRUE(user_data->GetLaunchParams().paths().empty());
  EXPECT_TRUE(user_data->GetLaunchParams().can_write().empty());
  EXPECT_TRUE(user_data->GetLaunchParams().dir().empty());
}

}  // namespace
}  // namespace web_app
