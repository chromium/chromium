// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>
#include <vector>

#include "chrome/browser/ui/web_applications/sub_apps_service_impl.h"

#include "base/containers/flat_set.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/commands/sub_app_install_command.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/webapps/browser/install_result_code.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

using blink::mojom::SubAppsServiceListResultPtr;
using blink::mojom::SubAppsServiceResult;

namespace web_app {

namespace {

constexpr const char kDomain[] = "www.foo.bar";
constexpr const char kSubDomain[] = "baz.foo.bar";
constexpr const char kDifferentDomain[] = "www.different-domain.com";
constexpr const char kParentAppPath[] = "/web_apps/basic.html";
constexpr const char kSubAppPath[] = "/web_apps/standalone/basic.html";
constexpr const char kSubAppPathMinimalUi[] =
    "/web_apps/standalone/basic.html?manifest=manifest_minimal_ui.json";
constexpr const char kSubAppPath2[] = "/web_apps/minimal_ui/basic.html";
constexpr const char kSubAppPath3[] = "/web_apps/site_d/basic.html";
constexpr const char kSubAppPathInvalid[] = "/invalid/sub/app/path.html";

}  // namespace

// There's one simple end-to-end test that actually calls the JS API interface,
// the rest test the mojo interface (since the first layer listening to the API
// calls is almost a direct passthrough to the mojo service).
//
// JS API interface tests are in
// third_party/blink/web_tests/external/wpt/subapps/.

class SubAppsServiceImplBrowserTest : public WebAppControllerBrowserTest {
 public:
  content::RenderFrameHost* render_frame_host(
      content::WebContents* web_contents = nullptr) {
    if (!web_contents) {
      web_contents = browser()->tab_strip_model()->GetActiveWebContents();
    }
    return web_contents->GetPrimaryMainFrame();
  }

  GURL GetURL(const std::string& url) {
    return https_server()->GetURL(kDomain, url);
  }

  void InstallParentApp() {
    parent_app_id_ = InstallPWA(GetURL(kParentAppPath));
  }

  void NavigateToParentApp() {
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GetURL(kParentAppPath)));
  }

  void UninstallParentApp() { UninstallWebApp(parent_app_id_); }

  std::vector<AppId> GetAllSubAppIds(const AppId& parent_app_id) {
    return provider().registrar().GetAllSubAppIds(parent_app_id);
  }

  void BindRemote(content::WebContents* web_contents = nullptr) {
    // Any navigation causes the remote to be destroyed (since the
    // render_frame_host that owns it gets destroyed.)
    SubAppsServiceImpl::CreateIfAllowed(render_frame_host(web_contents),
                                        remote_.BindNewPipeAndPassReceiver());
  }

  // Calls the Add() method on the mojo interface which is async, and waits for
  // it to finish.
  auto CallAdd(std::vector<std::pair<std::string, GURL>> subapps) {
    std::vector<blink::mojom::SubAppsServiceAddInfoPtr> mojom_subapps;
    for (const auto& [unhashed_app_id, install_url] : subapps) {
      blink::mojom::SubAppsServiceAddInfoPtr mojom_pair =
          blink::mojom::SubAppsServiceAddInfo::New();
      mojom_pair->unhashed_app_id = unhashed_app_id;
      mojom_pair->install_url = install_url;
      mojom_subapps.push_back(std::move(mojom_pair));
    }

    base::test::TestFuture<
        std::vector<blink::mojom::SubAppsServiceAddResultPtr>>
        future;
    remote_->Add(std::move(mojom_subapps), future.GetCallback());
    return future.Take();
  }

  // Calls the List() method on the mojo interface which is async, and waits for
  // it to finish.
  SubAppsServiceListResultPtr CallList() {
    base::test::TestFuture<SubAppsServiceListResultPtr> future;
    remote_->List(future.GetCallback());
    return future.Take();
  }

  // Calls the Remove() method on the mojo interface which is async, and waits
  // for it to finish.
  SubAppsServiceResult CallRemove(const std::string& unhashed_app_id) {
    base::test::TestFuture<SubAppsServiceResult> future;
    remote_->Remove(unhashed_app_id, future.GetCallback());
    return future.Get();
  }

  std::vector<std::pair<std::string, blink::mojom::SubAppsServiceAddResultCode>>
  ResultsFromMojo(std::vector<blink::mojom::SubAppsServiceAddResultPtr> pairs) {
    std::vector<
        std::pair<std::string, blink::mojom::SubAppsServiceAddResultCode>>
        subapps;
    for (const auto& pair : pairs) {
      subapps.emplace_back(pair->unhashed_app_id, pair->result_code);
    }
    return subapps;
  }

  std::vector<blink::mojom::SubAppsServiceAddResultPtr> Result(
      blink::mojom::SubAppsServiceAddResultCode result_code,
      UnhashedAppId sub_app_id) {
    blink::mojom::SubAppsServiceAddResultPtr mojom_pair =
        blink::mojom::SubAppsServiceAddResult::New();
    mojom_pair->unhashed_app_id = sub_app_id;
    mojom_pair->result_code = result_code;
    std::vector<blink::mojom::SubAppsServiceAddResultPtr> result;
    result.emplace_back(std::move(mojom_pair));
    return result;
  }

 protected:
  base::test::ScopedFeatureList features_{blink::features::kDesktopPWAsSubApps};
  AppId parent_app_id_;
  mojo::Remote<blink::mojom::SubAppsService> remote_;
};

// Simple end-to-end test for add().
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, EndToEndAdd) {
  NavigateToParentApp();
  InstallParentApp();
  EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id_).size());

  GURL kSubAppUrl1 = GetURL(kSubAppPath);
  UnhashedAppId unhashed_sub_app_id_1 =
      GenerateAppIdUnhashed(/*manifest_id=*/absl::nullopt, kSubAppUrl1);
  GURL kSubAppUrl2 = GetURL(kSubAppPath2);
  UnhashedAppId unhashed_sub_app_id_2 =
      GenerateAppIdUnhashed(/*manifest_id=*/absl::nullopt, kSubAppUrl2);

  std::string command = "navigator.subApps.add({\"" + unhashed_sub_app_id_1 +
                        "\":{\"install_url\":\"" + kSubAppUrl1.spec() +
                        "\"},\"" + unhashed_sub_app_id_2 +
                        "\":{\"install_url\":\"" + kSubAppUrl2.spec() + "\"}})";

  EXPECT_TRUE(ExecJs(render_frame_host(), command));
  EXPECT_EQ(2ul, GetAllSubAppIds(parent_app_id_).size());
}

// End-to-end test for add() with one succeeding and one failing install.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, EndToEndAddInvalidPath) {
  NavigateToParentApp();
  InstallParentApp();
  EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id_).size());

  const GURL kSubAppUrl = GetURL(kSubAppPath);
  UnhashedAppId unhashed_sub_app_id =
      GenerateAppIdUnhashed(/*manifest_id=*/absl::nullopt, kSubAppUrl);
  // Invalid app that should fail because the URL cannot be loaded.
  const GURL kInvalidSubAppUrl = GetURL(kSubAppPathInvalid);
  UnhashedAppId unhashed_invalid_sub_app_id =
      GenerateAppIdUnhashed(/*manifest_id=*/absl::nullopt, kInvalidSubAppUrl);

  std::string command = "navigator.subApps.add({\"" + unhashed_sub_app_id +
                        "\":{\"install_url\":\"" + kSubAppUrl.spec() +
                        "\"},\"" + unhashed_invalid_sub_app_id +
                        "\":{\"install_url\":\"" + kInvalidSubAppUrl.spec() +
                        "\"}})";

  // Add call promise should be rejected because an install failed.
  EXPECT_FALSE(ExecJs(render_frame_host(), command));
  EXPECT_EQ(1ul, GetAllSubAppIds(parent_app_id_).size());
}

// Add a single sub-app and verify all sorts of things.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, AddSingle) {
  // Dependency graph:
  // NavigateToParentApp --> BindRemote --> CallAdd
  //                   \---------------->/
  // InstallParentApp ----------------->/
  NavigateToParentApp();
  BindRemote();
  InstallParentApp();
  EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id_).size());

  GURL kSubAppUrl = GetURL(kSubAppPath);
  UnhashedAppId unhashed_sub_app_id =
      GenerateAppIdUnhashed(/*manifest_id=*/absl::nullopt, kSubAppUrl);
  AppId sub_app_id = GenerateAppIdFromUnhashed(unhashed_sub_app_id);

  EXPECT_EQ(
      Result(blink::mojom::SubAppsServiceAddResultCode::kSuccessNewInstall,
             unhashed_sub_app_id),
      CallAdd({{unhashed_sub_app_id, kSubAppUrl}}));

  // Verify a bunch of things for the newly installed sub-app.
  EXPECT_TRUE(provider().registrar().IsInstalled(sub_app_id));
  EXPECT_TRUE(provider().registrar().IsLocallyInstalled(sub_app_id));
  EXPECT_EQ(DisplayMode::kStandalone,
            provider().registrar().GetAppEffectiveDisplayMode(sub_app_id));

  const WebApp* sub_app = provider().registrar().GetAppById(sub_app_id);
  EXPECT_EQ(parent_app_id_, sub_app->parent_app_id());
  EXPECT_EQ(std::vector<AppId>{sub_app->app_id()},
            GetAllSubAppIds(parent_app_id_));
  EXPECT_TRUE(sub_app->IsSubAppInstalledApp());
  EXPECT_TRUE(sub_app->CanUserUninstallWebApp());
  EXPECT_EQ(sub_app->start_url(), kSubAppUrl);
  if (provider().ui_manager().CanAddAppToQuickLaunchBar()) {
    EXPECT_FALSE(provider().ui_manager().IsAppInQuickLaunchBar(sub_app_id));
  }
}

// Add one sub-app, verify count is one. Add it again, still same count. Add a
// second sub-app, verify count is two.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, AddTwo) {
  NavigateToParentApp();
  InstallParentApp();

  BindRemote();

  EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id_).size());

  GURL kSubAppUrl1 = GetURL(kSubAppPath);
  UnhashedAppId unhashed_sub_app_id_1 =
      GenerateAppIdUnhashed(/*manifest_id=*/absl::nullopt, kSubAppUrl1);

  // Add first sub app.
  EXPECT_EQ(
      Result(blink::mojom::SubAppsServiceAddResultCode::kSuccessNewInstall,
             unhashed_sub_app_id_1),
      CallAdd({{unhashed_sub_app_id_1, kSubAppUrl1}}));
  EXPECT_EQ(1ul, GetAllSubAppIds(parent_app_id_).size());

  // Try to add first sub app again.
  EXPECT_EQ(
      Result(
          blink::mojom::SubAppsServiceAddResultCode::kSuccessAlreadyInstalled,
          unhashed_sub_app_id_1),
      CallAdd({{unhashed_sub_app_id_1, kSubAppUrl1}}));
  EXPECT_EQ(1ul, GetAllSubAppIds(parent_app_id_).size());

  GURL kSubAppUrl2 = GetURL(kSubAppPath2);
  UnhashedAppId unhashed_sub_app_id_2 =
      GenerateAppIdUnhashed(/*manifest_id=*/absl::nullopt, kSubAppUrl2);

  // Add second sub app.
  EXPECT_EQ(
      Result(blink::mojom::SubAppsServiceAddResultCode::kSuccessNewInstall,
             unhashed_sub_app_id_2),
      CallAdd({{unhashed_sub_app_id_2, kSubAppUrl2}}));
  EXPECT_EQ(2ul, GetAllSubAppIds(parent_app_id_).size());
}

// Verify that a list of sub-apps in Add are all installed correctly.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, AddList) {
  NavigateToParentApp();
  InstallParentApp();
  BindRemote();

  GURL kSubAppUrl1 = GetURL(kSubAppPath);
  UnhashedAppId unhashed_sub_app_id_1 =
      GenerateAppIdUnhashed(/*manifest_id=*/absl::nullopt, kSubAppUrl1);
  GURL kSubAppUrl2 = GetURL(kSubAppPath2);
  UnhashedAppId unhashed_sub_app_id_2 =
      GenerateAppIdUnhashed(/*manifest_id=*/absl::nullopt, kSubAppUrl2);
  GURL kSubAppUrl3 = GetURL(kSubAppPath3);
  UnhashedAppId unhashed_sub_app_id_3 =
      GenerateAppIdUnhashed(/*manifest_id=*/absl::nullopt, kSubAppUrl3);

  std::vector<std::pair<std::string, GURL>> subapps = {
      {unhashed_sub_app_id_1, kSubAppUrl1},
      {unhashed_sub_app_id_2, kSubAppUrl2},
      {unhashed_sub_app_id_3, kSubAppUrl3}};

  std::vector<std::pair<std::string, blink::mojom::SubAppsServiceAddResultCode>>
      actual_results = ResultsFromMojo(CallAdd(std::move(subapps)));

  EXPECT_THAT(
      actual_results,
      testing::UnorderedElementsAre(
          std::pair{
              unhashed_sub_app_id_1,
              blink::mojom::SubAppsServiceAddResultCode::kSuccessNewInstall},
          std::pair{
              unhashed_sub_app_id_2,
              blink::mojom::SubAppsServiceAddResultCode::kSuccessNewInstall},
          std::pair{
              unhashed_sub_app_id_3,
              blink::mojom::SubAppsServiceAddResultCode::kSuccessNewInstall}));

  EXPECT_EQ(3ul, GetAllSubAppIds(parent_app_id_).size());
}

// Verify that Add works if PWA is launched as standalone window.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, AddStandaloneWindow) {
  NavigateToParentApp();
  InstallParentApp();
  content::WebContents* web_contents = OpenApplication(parent_app_id_);
  BindRemote(web_contents);

  GURL kSubAppUrl = GetURL(kSubAppPath);
  UnhashedAppId unhashed_sub_app_id =
      GenerateAppIdUnhashed(/*manifest_id=*/absl::nullopt, kSubAppUrl);

  EXPECT_EQ(
      Result(blink::mojom::SubAppsServiceAddResultCode::kSuccessNewInstall,
             unhashed_sub_app_id),
      CallAdd({{unhashed_sub_app_id, kSubAppUrl}}));
}

// Verify that a list of both correct and incorrect subapps returns the correct
// result.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest,
                       AddListWithInvalidSubApps) {
  NavigateToParentApp();
  InstallParentApp();
  BindRemote();

  GURL kSubAppUrl1 = GetURL(kSubAppPath);
  UnhashedAppId unhashed_sub_app_id_1 =
      GenerateAppIdUnhashed(/*manifest_id=*/absl::nullopt, kSubAppUrl1);
  GURL kSubAppUrl2 = GetURL(kSubAppPathInvalid);
  UnhashedAppId unhashed_sub_app_id_2 =
      GenerateAppIdUnhashed(/*manifest_id=*/absl::nullopt, kSubAppUrl2);
  GURL kSubAppUrl3 = GetURL(kSubAppPath3);
  UnhashedAppId unhashed_sub_app_id_3 =
      GenerateAppIdUnhashed(/*manifest_id=*/absl::nullopt, kSubAppUrl3);

  std::vector<std::pair<std::string, GURL>> subapps = {
      {unhashed_sub_app_id_1, kSubAppUrl1},
      {unhashed_sub_app_id_2, kSubAppUrl2},
      {unhashed_sub_app_id_3, kSubAppUrl3}};

  std::vector<std::pair<std::string, blink::mojom::SubAppsServiceAddResultCode>>
      actual_results = ResultsFromMojo(CallAdd(std::move(subapps)));

  EXPECT_THAT(
      actual_results,
      testing::UnorderedElementsAre(
          std::pair{
              unhashed_sub_app_id_1,
              blink::mojom::SubAppsServiceAddResultCode::kSuccessNewInstall},
          std::pair{unhashed_sub_app_id_2,
                    blink::mojom::SubAppsServiceAddResultCode::kFailure},
          std::pair{
              unhashed_sub_app_id_3,
              blink::mojom::SubAppsServiceAddResultCode::kSuccessNewInstall}));
  EXPECT_EQ(2ul, GetAllSubAppIds(parent_app_id_).size());
}

// End-to-end. Test that adding a sub-app from a different origin or from a
// different domain fails.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest,
                       EndToEndAddFailDifferentOrigin) {
  NavigateToParentApp();
  InstallParentApp();
  EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id_).size());

  GURL different_origin = https_server()->GetURL(kSubDomain, kSubAppPath);
  UnhashedAppId unhashed_sub_app_id =
      GenerateAppIdUnhashed(/*manifest_id=*/absl::nullopt, different_origin);
  std::string command = "navigator.subApps.add({\"" + unhashed_sub_app_id +
                        "\":{\"install_url\":\"" + different_origin.spec() +
                        "\"}})";

  // EXPECT_FALSE because this returns an error.
  EXPECT_FALSE(ExecJs(render_frame_host(), command));

  EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id_).size());

  GURL different_domain =
      https_server()->GetURL(kDifferentDomain, kSubAppPath2);
  unhashed_sub_app_id =
      GenerateAppIdUnhashed(/*manifest_id=*/absl::nullopt, different_domain);
  command = "navigator.subApps.add({\"" + unhashed_sub_app_id + "\":\"" +
            different_domain.spec() + "\"})";

  EXPECT_FALSE(ExecJs(render_frame_host(), command));
  EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id_).size());
}

// Add call should fail if the parent app isn't installed.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest,
                       AddFailParentAppNotInstalled) {
  NavigateToParentApp();
  BindRemote();

  GURL kSubAppUrl = GetURL(kSubAppPath);
  UnhashedAppId unhashed_sub_app_id =
      GenerateAppIdUnhashed(/*manifest_id=*/absl::nullopt, kSubAppUrl);

  std::vector<blink::mojom::SubAppsServiceAddResultPtr> results;

  EXPECT_EQ(std::move(results), CallAdd({{unhashed_sub_app_id, kSubAppUrl}}));
}

// Add call should fail if the parent app is uninstalled between the add call
// and the start of the command.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest,
                       AddFailParentAppWasUninstalled) {
  base::RunLoop loop;
  NavigateToParentApp();
  GURL kSubAppUrl = GetURL(kSubAppPath);
  UnhashedAppId unhashed_sub_app_id =
      GenerateAppIdUnhashed(/*manifest_id=*/absl::nullopt, kSubAppUrl);

  AppInstallResults results;
  base::flat_set<AppId> app_ids = {
      GenerateAppIdFromUnhashed(unhashed_sub_app_id)};
  std::vector<std::pair<UnhashedAppId, GURL>> subapps = {
      {unhashed_sub_app_id, kSubAppUrl}};
  auto install_command = std::make_unique<SubAppInstallCommand>(
      &provider().install_manager(), &provider().registrar(), parent_app_id_,
      std::move(subapps), app_ids,
      base::BindLambdaForTesting([&](AppInstallResults arg_results) {
        results = arg_results;
        loop.Quit();
      }));
  provider().command_manager().ScheduleCommand(std::move(install_command));
  loop.Run();

  std::vector<blink::mojom::SubAppsServiceAddResultPtr> mojom_results;
  blink::mojom::SubAppsServiceAddResultPtr mojom_pair =
      blink::mojom::SubAppsServiceAddResult::New();
  mojom_pair->unhashed_app_id = results[0].first;
  mojom_pair->result_code = results[0].second;
  mojom_results.push_back(std::move(mojom_pair));

  EXPECT_EQ(
      Result(blink::mojom::SubAppsServiceAddResultCode::kParentAppUninstalled,
             unhashed_sub_app_id),
      mojom_results);
  EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id_).size());
}

// Add call should fail if the call wasn't made from the context of parent app.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest,
                       AddFailNotInParentAppContext) {
  InstallParentApp();
  BindRemote();

  GURL kSubAppUrl = GetURL(kSubAppPath);
  UnhashedAppId unhashed_sub_app_id =
      GenerateAppIdUnhashed(/*manifest_id=*/absl::nullopt, kSubAppUrl);

  std::vector<blink::mojom::SubAppsServiceAddResultPtr> results;

  EXPECT_EQ(std::move(results), CallAdd({{unhashed_sub_app_id, kSubAppUrl}}));
}

// Verify that Add fails for an empty list.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, AddEmptyList) {
  NavigateToParentApp();
  InstallParentApp();
  BindRemote();

  std::vector<blink::mojom::SubAppsServiceAddResultPtr> results;

  EXPECT_EQ(std::move(results), CallAdd({}));
  EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id_).size());
}

// Verify that a sub-app with mismatched install-path and id is not installed
// and correct error is returned in Add.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, AddIncorrectId) {
  NavigateToParentApp();
  InstallParentApp();
  BindRemote();

  GURL kSubAppUrl = GetURL(kSubAppPath);
  UnhashedAppId unhashed_sub_app_id = "https://www.invalid.com/";

  EXPECT_EQ(
      Result(
          blink::mojom::SubAppsServiceAddResultCode::kExpectedAppIdCheckFailed,
          unhashed_sub_app_id),
      CallAdd({{unhashed_sub_app_id, kSubAppUrl}}));
  EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id_).size());
}

// Verify that a sub-app with an unhashed app id that is not a valid URL fails.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, AddDifferentOrigin) {
  NavigateToParentApp();
  InstallParentApp();
  BindRemote();

  GURL different_origin = https_server()->GetURL(kSubDomain, kSubAppPath);
  UnhashedAppId unhashed_sub_app_id =
      GenerateAppIdUnhashed(/*manifest_id=*/absl::nullopt, different_origin);

  CallAdd({{unhashed_sub_app_id, different_origin}});
  EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id_).size());
}

// Verify that a sub-app with an unhashed app id that is not a valid URL fails.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, AddInvalidId) {
  NavigateToParentApp();
  InstallParentApp();
  BindRemote();

  GURL kSubAppUrl = GetURL(kSubAppPath);
  UnhashedAppId unhashed_sub_app_id = "invalid";

  CallAdd({{unhashed_sub_app_id, kSubAppUrl}});
  EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id_).size());
}

// Verify that Add fails for an invalid (non-existing) sub-app.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, AddNonExistent) {
  NavigateToParentApp();
  InstallParentApp();
  BindRemote();

  GURL kSubAppUrl = GetURL(kSubAppPathInvalid);
  UnhashedAppId unhashed_sub_app_id =
      GenerateAppIdUnhashed(/*manifest_id=*/absl::nullopt, kSubAppUrl);

  EXPECT_EQ(Result(blink::mojom::SubAppsServiceAddResultCode::kFailure,
                   unhashed_sub_app_id),
            CallAdd({{unhashed_sub_app_id, kSubAppUrl}}));
  EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id_).size());
}

// Verify that uninstalling an app with sub-apps causes sub-apps to be
// uninstalled as well.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest,
                       UninstallingParentAppUninstallsSubApps) {
  NavigateToParentApp();
  InstallParentApp();
  BindRemote();

  // Verify that subapps are installed.
  UnhashedAppId unhashed_sub_app_id_1 =
      GenerateAppIdUnhashed(/*manifest_id=*/absl::nullopt, GetURL(kSubAppPath));
  UnhashedAppId unhashed_sub_app_id_2 = GenerateAppIdUnhashed(
      /*manifest_id=*/absl::nullopt, GetURL(kSubAppPath2));
  UnhashedAppId unhashed_sub_app_id_3 = GenerateAppIdUnhashed(
      /*manifest_id=*/absl::nullopt, GetURL(kSubAppPath3));

  EXPECT_EQ(
      Result(blink::mojom::SubAppsServiceAddResultCode::kSuccessNewInstall,
             unhashed_sub_app_id_1),
      CallAdd({{unhashed_sub_app_id_1, GetURL(kSubAppPath)}}));
  EXPECT_EQ(
      Result(blink::mojom::SubAppsServiceAddResultCode::kSuccessNewInstall,
             unhashed_sub_app_id_2),
      CallAdd({{unhashed_sub_app_id_2, GetURL(kSubAppPath2)}}));
  EXPECT_EQ(
      Result(blink::mojom::SubAppsServiceAddResultCode::kSuccessNewInstall,
             unhashed_sub_app_id_3),
      CallAdd({{unhashed_sub_app_id_3, GetURL(kSubAppPath3)}}));

  EXPECT_TRUE(provider().registrar().IsInstalled(
      GenerateAppIdFromUnhashed(unhashed_sub_app_id_1)));
  EXPECT_TRUE(provider().registrar().IsInstalled(
      GenerateAppIdFromUnhashed(unhashed_sub_app_id_2)));
  EXPECT_TRUE(provider().registrar().IsInstalled(
      GenerateAppIdFromUnhashed(unhashed_sub_app_id_3)));

  UninstallParentApp();
  // Verify that both parent app and sub apps are no longer installed.
  EXPECT_FALSE(provider().registrar().IsInstalled(parent_app_id_));
  EXPECT_FALSE(provider().registrar().IsInstalled(
      GenerateAppIdFromUnhashed(unhashed_sub_app_id_1)));
  EXPECT_FALSE(provider().registrar().IsInstalled(
      GenerateAppIdFromUnhashed(unhashed_sub_app_id_2)));
  EXPECT_FALSE(provider().registrar().IsInstalled(
      GenerateAppIdFromUnhashed(unhashed_sub_app_id_3)));
}

// Make sure the Add API can't force manifest update. Add sub-app, verify
// display mode, then add the same one again with different display mode in the
// manifest, and verify that it didn't change.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, AddDoesntForceReinstall) {
  NavigateToParentApp();
  InstallParentApp();
  BindRemote();

  GURL kSubAppUrl = GetURL(kSubAppPath);
  UnhashedAppId unhashed_sub_app_id =
      GenerateAppIdUnhashed(/*manifest_id=*/absl::nullopt, kSubAppUrl);

  EXPECT_EQ(
      Result(blink::mojom::SubAppsServiceAddResultCode::kSuccessNewInstall,
             unhashed_sub_app_id),
      CallAdd({{unhashed_sub_app_id, kSubAppUrl}}));
  EXPECT_EQ(DisplayMode::kStandalone,
            provider().registrar().GetAppEffectiveDisplayMode(
                GenerateAppIdFromUnhashed(unhashed_sub_app_id)));

  GURL kSubAppWithMinialUiUrl = GetURL(kSubAppPathMinimalUi);

  EXPECT_EQ(
      Result(
          blink::mojom::SubAppsServiceAddResultCode::kSuccessAlreadyInstalled,
          unhashed_sub_app_id),
      CallAdd({{unhashed_sub_app_id, kSubAppWithMinialUiUrl}}));
  EXPECT_EQ(DisplayMode::kStandalone,
            provider().registrar().GetAppEffectiveDisplayMode(
                GenerateAppIdFromUnhashed(unhashed_sub_app_id)));
}

// Verify that uninstalling an app that has a sub-app with more than one
// install source only removes the "sub-app" install source for that sub-app
// but does not uninstall it.
IN_PROC_BROWSER_TEST_F(
    SubAppsServiceImplBrowserTest,
    StandaloneAppStaysInstalledAfterUpgradedParentUninstall) {
  // Install app as standalone app.
  AppId standalone_app_id = InstallPWA(GetURL(kSubAppPath2));
  UnhashedAppId unhashed_standalone_app_id = GenerateAppIdUnhashed(
      /*manifest_id=*/absl::nullopt, GetURL(kSubAppPath2));

  NavigateToParentApp();
  InstallParentApp();
  BindRemote();

  // Add normal subapp to verify standalone app install/uninstall does
  // not affect normal sub app uninstalls.
  UnhashedAppId unhashed_sub_app_id =
      GenerateAppIdUnhashed(/*manifest_id=*/absl::nullopt, GetURL(kSubAppPath));
  EXPECT_EQ(
      Result(blink::mojom::SubAppsServiceAddResultCode::kSuccessNewInstall,
             unhashed_sub_app_id),
      CallAdd({{unhashed_sub_app_id, GetURL(kSubAppPath)}}));

  EXPECT_TRUE(provider().registrar().IsInstalled(
      GenerateAppIdFromUnhashed(unhashed_sub_app_id)));

  // Add standalone app as sub-app.
  const WebApp* standalone_app =
      provider().registrar().GetAppById(standalone_app_id);
  EXPECT_EQ(
      Result(blink::mojom::SubAppsServiceAddResultCode::kSuccessNewInstall,
             unhashed_standalone_app_id),
      CallAdd({{unhashed_standalone_app_id, GetURL(kSubAppPath2)}}));

  // Verify that it is now installed and registered as a subapp.

  EXPECT_EQ(parent_app_id_, standalone_app->parent_app_id());
  EXPECT_FALSE(standalone_app->HasOnlySource(WebAppManagement::kSync));
  EXPECT_TRUE(standalone_app->IsSubAppInstalledApp());

  UninstallParentApp();

  // Verify that normal sub-app is uninstalled.
  EXPECT_FALSE(provider().registrar().IsInstalled(
      GenerateAppIdFromUnhashed(unhashed_sub_app_id)));

  // Verify that previous standalone is still installed.
  EXPECT_TRUE(provider().registrar().IsInstalled(standalone_app_id));

  // Verify that there are no apps registered as parent app's sub apps.
  EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id_).size());

  EXPECT_EQ(absl::nullopt, standalone_app->parent_app_id());

  // Verify that the standalone app no longer has the sub-app install source.
  EXPECT_TRUE(standalone_app->HasOnlySource(WebAppManagement::kSync));
}

// List call returns the correct value for three sub-apps.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, ListSuccess) {
  NavigateToParentApp();
  InstallParentApp();
  BindRemote();

  // Empty list before adding any sub-apps.
  SubAppsServiceListResultPtr result = CallList();
  EXPECT_EQ(SubAppsServiceResult::kSuccess, result->code);
  EXPECT_EQ(std::vector<std::string>{}, result->sub_app_ids);

  UnhashedAppId unhashed_sub_app_id_1 =
      GenerateAppIdUnhashed(/*manifest_id=*/absl::nullopt, GetURL(kSubAppPath));
  UnhashedAppId unhashed_sub_app_id_2 = GenerateAppIdUnhashed(
      /*manifest_id=*/absl::nullopt, GetURL(kSubAppPath2));
  UnhashedAppId unhashed_sub_app_id_3 = GenerateAppIdUnhashed(
      /*manifest_id=*/absl::nullopt, GetURL(kSubAppPath3));

  EXPECT_EQ(
      Result(blink::mojom::SubAppsServiceAddResultCode::kSuccessNewInstall,
             unhashed_sub_app_id_1),
      CallAdd({{unhashed_sub_app_id_1, GetURL(kSubAppPath)}}));
  EXPECT_EQ(
      Result(blink::mojom::SubAppsServiceAddResultCode::kSuccessNewInstall,
             unhashed_sub_app_id_2),
      CallAdd({{unhashed_sub_app_id_2, GetURL(kSubAppPath2)}}));
  EXPECT_EQ(
      Result(blink::mojom::SubAppsServiceAddResultCode::kSuccessNewInstall,
             unhashed_sub_app_id_3),
      CallAdd({{unhashed_sub_app_id_3, GetURL(kSubAppPath3)}}));

  // We need to use a set for comparison because the ordering changes between
  // invocations (due to embedded test server using a random port each time).
  base::flat_set<std::string> expected_set = {
      GetURL(kSubAppPath).spec(),
      GetURL(kSubAppPath2).spec(),
      GetURL(kSubAppPath3).spec(),
  };
  result = CallList();
  EXPECT_EQ(SubAppsServiceResult::kSuccess, result->code);
  base::flat_set<std::string> actual_set(result->sub_app_ids.begin(),
                                         result->sub_app_ids.end());
  // We see all three sub-apps now.
  EXPECT_EQ(expected_set, actual_set);
}

// Verify that the list call doesn't return a non-sub-apps installed app.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest,
                       ListDoesntReturnNonSubApp) {
  // Regular install.
  InstallPWA(GetURL(kSubAppPath));

  NavigateToParentApp();
  InstallParentApp();
  BindRemote();

  UnhashedAppId unhashed_sub_app_id_2 = GenerateAppIdUnhashed(
      /*manifest_id=*/absl::nullopt, GetURL(kSubAppPath2));
  // Sub-app install.
  EXPECT_EQ(
      Result(blink::mojom::SubAppsServiceAddResultCode::kSuccessNewInstall,
             unhashed_sub_app_id_2),
      CallAdd({{unhashed_sub_app_id_2, GetURL(kSubAppPath2)}}));
  // Should only see the sub-app one here, not the standalone.
  SubAppsServiceListResultPtr result = CallList();
  EXPECT_EQ(SubAppsServiceResult::kSuccess, result->code);
  EXPECT_EQ(std::vector<std::string>{GetURL(kSubAppPath2).spec()},
            result->sub_app_ids);
}

// List call returns failure if the parent app isn't installed.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest,
                       ListFailParentAppNotInstalled) {
  NavigateToParentApp();
  BindRemote();

  SubAppsServiceListResultPtr result = CallList();
  EXPECT_EQ(SubAppsServiceResult::kFailure, result->code);
  EXPECT_EQ(std::vector<std::string>(), result->sub_app_ids);
}

// Remove works with one app.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, RemoveOneApp) {
  InstallParentApp();
  NavigateToParentApp();
  BindRemote();

  UnhashedAppId unhashed_app_id = GetURL(kSubAppPath).spec();
  AppId app_id = GenerateAppIdFromUnhashed(unhashed_app_id);

  EXPECT_EQ(
      Result(blink::mojom::SubAppsServiceAddResultCode::kSuccessNewInstall,
             unhashed_app_id),
      CallAdd({{unhashed_app_id, GetURL(kSubAppPath)}}));
  EXPECT_EQ(1ul, GetAllSubAppIds(parent_app_id_).size());
  EXPECT_TRUE(provider().registrar().IsInstalled(app_id));

  EXPECT_EQ(SubAppsServiceResult::kSuccess, CallRemove(unhashed_app_id));
  EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id_).size());
  EXPECT_FALSE(provider().registrar().IsInstalled(app_id));
}

// Remove fails for a regular installed app.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, RemoveFailRegularApp) {
  // Regular install.
  InstallPWA(GetURL(kSubAppPath));

  InstallParentApp();
  NavigateToParentApp();
  BindRemote();

  UnhashedAppId unhashed_app_id = GetURL(kSubAppPath).spec();
  EXPECT_EQ(SubAppsServiceResult::kFailure, CallRemove(unhashed_app_id));
}

// Remove fails for a sub-app with a different parent_app_id.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, RemoveFailWrongParent) {
  // SubApp plays the parent app here, SubApp2 is its sub-app, SubApp3 is the
  // other "parent app".
  AppId parent_app = InstallPWA(GetURL(kSubAppPath));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetURL(kSubAppPath)));
  BindRemote();

  GURL kSubAppUrl = GetURL(kSubAppPath);
  UnhashedAppId unhashed_sub_app_id =
      GenerateAppIdUnhashed(/*manifest_id=*/absl::nullopt, kSubAppUrl);

  EXPECT_EQ(
      Result(blink::mojom::SubAppsServiceAddResultCode::kSuccessNewInstall,
             unhashed_sub_app_id),
      CallAdd({{unhashed_sub_app_id, kSubAppUrl}}));

  AppId second_parent_app = InstallPWA(GetURL(kSubAppPath3));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetURL(kSubAppPath3)));
  remote_.reset();
  BindRemote();

  EXPECT_EQ(SubAppsServiceResult::kFailure, CallRemove(unhashed_sub_app_id));
}

// Remove call returns failure if the calling app isn't installed.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest,
                       RemoveFailCallingAppNotInstalled) {
  NavigateToParentApp();
  BindRemote();

  UnhashedAppId unhashed_app_id = GetURL(kSubAppPath).spec();
  EXPECT_EQ(SubAppsServiceResult::kFailure, CallRemove(unhashed_app_id));
}

// Remove doesn't crash with an invalid unhashed_app_id.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest,
                       RemoveInvalidArgDoesntCrash) {
  InstallParentApp();
  NavigateToParentApp();
  BindRemote();

  // Invalid because it isn't a proper URL.
  UnhashedAppId unhashed_app_id = "invalid";
  EXPECT_EQ(SubAppsServiceResult::kFailure, CallRemove(unhashed_app_id));

  // Shouldn't crash.
}

}  // namespace web_app
