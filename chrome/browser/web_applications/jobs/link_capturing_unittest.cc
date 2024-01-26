// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_future.h"
#include "chrome/browser/apps/link_capturing/link_capturing_feature_test_support.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "components/webapps/common/web_app_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {
namespace {

class LinkCapturingJobTest : public WebAppTest,
                             public testing::WithParamInterface<bool> {
 public:
  const GURL kTestAppUrl = GURL("https://example.com/index.html");
  const GURL kTestOverlappingAppUrl = GURL("https://example.com/index2.html");
  const GURL kTestAppCapturablePage = GURL("https://example.com/page.html");

  LinkCapturingJobTest() {
    feature_list_.InitWithFeaturesAndParameters(
        apps::test::GetFeaturesToEnableLinkCapturingUX(
            /*override_captures_by_default=*/GetParam()),
        {});
  }
  ~LinkCapturingJobTest() override = default;

  bool LinkCapturingEnabledByDefault() const { return GetParam(); }

  void SetUp() override {
    WebAppTest::SetUp();
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  WebAppProvider* provider() { return WebAppProvider::GetForTest(profile()); }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(LinkCapturingJobTest, SingleAppEnabled) {
  webapps::AppId app_id = test::InstallWebApp(
      profile(), WebAppInstallInfo::CreateWithStartUrlForTesting(kTestAppUrl));

  // For LinkCapturingEnabledByDefault(), simply test the default & that
  // enabling it doesn't change anything.
  EXPECT_TRUE(provider()->registrar_unsafe().IsLinkCapturableByApp(
      app_id, kTestAppCapturablePage));
  EXPECT_EQ(LinkCapturingEnabledByDefault(),
            provider()->registrar_unsafe().CapturesLinksInScope(app_id));
  EXPECT_EQ(
      LinkCapturingEnabledByDefault() ? std::optional(app_id) : std::nullopt,
      provider()->registrar_unsafe().FindAppThatCapturesLinksInScope(
          kTestAppCapturablePage));

  base::test::TestFuture<void> preference_set;
  provider()->scheduler().SetAppCapturesSupportedLinksDisableOverlapping(
      app_id, /*set_to_preferred=*/true, preference_set.GetCallback());
  ASSERT_TRUE(preference_set.Wait());

  EXPECT_EQ(provider()
                ->registrar_unsafe()
                .GetAppById(app_id)
                ->user_link_capturing_preference(),
            proto::LinkCapturingUserPreference::CAPTURE_SUPPORTED_LINKS);

  EXPECT_TRUE(provider()->registrar_unsafe().IsLinkCapturableByApp(
      app_id, kTestAppCapturablePage));
  EXPECT_TRUE(provider()->registrar_unsafe().CapturesLinksInScope(app_id));
  EXPECT_EQ(app_id,
            provider()->registrar_unsafe().FindAppThatCapturesLinksInScope(
                kTestAppCapturablePage));
}

TEST_P(LinkCapturingJobTest, SingleAppDisabled) {
  webapps::AppId app_id = test::InstallWebApp(
      profile(), WebAppInstallInfo::CreateWithStartUrlForTesting(kTestAppUrl));

  base::test::TestFuture<void> preference_set;
  provider()->scheduler().SetAppCapturesSupportedLinksDisableOverlapping(
      app_id, /*set_to_preferred=*/false, preference_set.GetCallback());
  ASSERT_TRUE(preference_set.Wait());

  EXPECT_EQ(provider()
                ->registrar_unsafe()
                .GetAppById(app_id)
                ->user_link_capturing_preference(),
            proto::LinkCapturingUserPreference::DO_NOT_CAPTURE_SUPPORTED_LINKS);
  EXPECT_TRUE(provider()->registrar_unsafe().IsLinkCapturableByApp(
      app_id, kTestAppCapturablePage));
  EXPECT_FALSE(provider()->registrar_unsafe().CapturesLinksInScope(app_id));
  EXPECT_EQ(std::nullopt,
            provider()->registrar_unsafe().FindAppThatCapturesLinksInScope(
                kTestAppCapturablePage));
}

TEST_P(LinkCapturingJobTest, DisablesOtherApps) {
  webapps::AppId app1_id = test::InstallWebApp(
      profile(), WebAppInstallInfo::CreateWithStartUrlForTesting(kTestAppUrl));
  webapps::AppId app2_id = test::InstallWebApp(
      profile(),
      WebAppInstallInfo::CreateWithStartUrlForTesting(kTestOverlappingAppUrl));

  {
    base::test::TestFuture<void> preference_set;
    provider()->scheduler().SetAppCapturesSupportedLinksDisableOverlapping(
        app1_id, /*set_to_preferred=*/true, preference_set.GetCallback());
    ASSERT_TRUE(preference_set.Wait());
  }

  EXPECT_TRUE(provider()->registrar_unsafe().CapturesLinksInScope(app1_id));

  // This should disable link capturing on the first app, and enable it on the
  // second app.
  {
    base::test::TestFuture<void> preference_set;
    provider()->scheduler().SetAppCapturesSupportedLinksDisableOverlapping(
        app2_id, /*set_to_preferred=*/true, preference_set.GetCallback());
    ASSERT_TRUE(preference_set.Wait());
  }

  // The first app should have this disabled.
  EXPECT_FALSE(provider()->registrar_unsafe().CapturesLinksInScope(app1_id));

  // It should be set up on the second app.
  EXPECT_TRUE(provider()->registrar_unsafe().CapturesLinksInScope(app2_id));
  EXPECT_EQ(app2_id,
            provider()->registrar_unsafe().FindAppThatCapturesLinksInScope(
                kTestAppCapturablePage));
}

TEST_P(LinkCapturingJobTest, DefaultOnChoosesFirstApp) {
  if (!LinkCapturingEnabledByDefault()) {
    GTEST_SKIP();
  }
  webapps::AppId app1_id = test::InstallWebApp(
      profile(), WebAppInstallInfo::CreateWithStartUrlForTesting(kTestAppUrl));
  webapps::AppId app2_id = test::InstallWebApp(
      profile(),
      WebAppInstallInfo::CreateWithStartUrlForTesting(kTestOverlappingAppUrl));

  EXPECT_TRUE(provider()->registrar_unsafe().CapturesLinksInScope(app1_id));
  EXPECT_FALSE(provider()->registrar_unsafe().CapturesLinksInScope(app2_id));
}

TEST_P(LinkCapturingJobTest, ExplicitDisablingOverrideDefaultBehavior) {
  webapps::AppId app1_id = test::InstallWebApp(
      profile(), WebAppInstallInfo::CreateWithStartUrlForTesting(kTestAppUrl));
  webapps::AppId app2_id = test::InstallWebApp(
      profile(),
      WebAppInstallInfo::CreateWithStartUrlForTesting(kTestOverlappingAppUrl));

  // Disabling the second should always cause it to be disabled, even in
  // 'default on' scenario after the first app is uninstalled.
  {
    base::test::TestFuture<void> preference_set;
    provider()->scheduler().SetAppCapturesSupportedLinksDisableOverlapping(
        app2_id, /*set_to_preferred=*/false, preference_set.GetCallback());
    ASSERT_TRUE(preference_set.Wait());
  }

  EXPECT_FALSE(provider()->registrar_unsafe().CapturesLinksInScope(app2_id));

  test::UninstallWebApp(profile(), app1_id);

  // The app remains disabled, if it we were "default on".
  EXPECT_FALSE(provider()->registrar_unsafe().CapturesLinksInScope(app2_id));
}

TEST_P(LinkCapturingJobTest, UninstallOverlappingRevertsToDefault) {
  webapps::AppId app1_id = test::InstallWebApp(
      profile(), WebAppInstallInfo::CreateWithStartUrlForTesting(kTestAppUrl));
  webapps::AppId app2_id = test::InstallWebApp(
      profile(),
      WebAppInstallInfo::CreateWithStartUrlForTesting(kTestOverlappingAppUrl));

  // Enable the first, which should explicitly disable the second.
  {
    base::test::TestFuture<void> preference_set;
    provider()->scheduler().SetAppCapturesSupportedLinksDisableOverlapping(
        app1_id, /*set_to_preferred=*/true, preference_set.GetCallback());
    ASSERT_TRUE(preference_set.Wait());
  }

  EXPECT_TRUE(provider()->registrar_unsafe().CapturesLinksInScope(app1_id));
  EXPECT_FALSE(provider()->registrar_unsafe().CapturesLinksInScope(app2_id));

  // Uninstalling the first should make the second app revert to the default.
  test::UninstallWebApp(profile(), app1_id);

  EXPECT_EQ(LinkCapturingEnabledByDefault(),
            provider()->registrar_unsafe().CapturesLinksInScope(app2_id));
}

INSTANTIATE_TEST_SUITE_P(,
                         LinkCapturingJobTest,
                         testing::Values(true, false),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "DefaultOn" : "DefaultOff";
                         });

}  // namespace
}  // namespace web_app
