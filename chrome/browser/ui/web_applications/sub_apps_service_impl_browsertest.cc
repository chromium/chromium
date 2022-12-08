// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>
#include <vector>

#include "chrome/browser/ui/web_applications/sub_apps_service_impl.h"

#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

using blink::mojom::SubAppsService;
using blink::mojom::SubAppsServiceAddInfo;
using blink::mojom::SubAppsServiceAddInfoPtr;
using blink::mojom::SubAppsServiceAddResultCode;
using blink::mojom::SubAppsServiceListInfo;
using blink::mojom::SubAppsServiceListInfoPtr;
using blink::mojom::SubAppsServiceListResultPtr;
using blink::mojom::SubAppsServiceResult;

namespace web_app {

namespace {

constexpr const char kDomain[] = "www.foo.bar";
constexpr const char kSubDomain[] = "baz.foo.bar";
constexpr const char kDifferentDomain[] = "www.different-domain.com";
constexpr const char kParentAppPath[] = "/web_apps/basic.html";
constexpr const char kSubAppPath[] = "/web_apps/standalone/basic.html";
constexpr const char kSubAppName[] = "Site A";
constexpr const char kSubAppPathMinimalUi[] =
    "/web_apps/standalone/basic.html?manifest=manifest_minimal_ui.json";
constexpr const char kSubAppPath2[] = "/web_apps/minimal_ui/basic.html";
constexpr const char kSubAppName2[] = "Site B";
constexpr const char kSubAppPath3[] = "/web_apps/site_d/basic.html";
constexpr const char kSubAppName3[] = "Site D";
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

  void UninstallParentAppBySource(WebAppManagement::Type source) {
    base::RunLoop run_loop;
    provider().install_finalizer().UninstallExternalWebApp(
        parent_app_id_, source,
        webapps::WebappUninstallSource::kParentUninstall,
        base::BindLambdaForTesting([&](webapps::UninstallResultCode code) {
          EXPECT_EQ(code, webapps::UninstallResultCode::kSuccess);
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  std::vector<AppId> GetAllSubAppIds(const AppId& parent_app_id) {
    return provider().registrar_unsafe().GetAllSubAppIds(parent_app_id);
  }

  void BindRemote(content::WebContents* web_contents = nullptr) {
    // Any navigation causes the remote to be destroyed (since the
    // render_frame_host that owns it gets destroyed.)
    SubAppsServiceImpl::CreateIfAllowed(render_frame_host(web_contents),
                                        remote_.BindNewPipeAndPassReceiver());
  }

  // Calls the Add() method on the mojo interface which is async, and waits for
  // it to finish.
  SubAppsServiceImpl::AddResultsMojo CallAdd(
      std::vector<std::pair<std::string, GURL>> subapps) {
    std::vector<SubAppsServiceAddInfoPtr> sub_apps_mojo;
    for (const auto& [unhashed_app_id, install_url] : subapps) {
      sub_apps_mojo.emplace_back(
          SubAppsServiceAddInfo::New(unhashed_app_id, install_url));
    }

    base::test::TestFuture<SubAppsServiceImpl::AddResultsMojo> future;
    remote_->Add(std::move(sub_apps_mojo), future.GetCallback());
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

  SubAppsServiceImpl::AddResults AddResultsFromMojo(
      SubAppsServiceImpl::AddResultsMojo add_results_mojo) {
    SubAppsServiceImpl::AddResults add_results;
    for (const auto& result : add_results_mojo) {
      add_results.emplace_back(result->unhashed_app_id, result->result_code);
    }
    return add_results;
  }

  SubAppsServiceImpl::AddResultsMojo AddResultMojo(
      UnhashedAppId unhashed_app_id,
      SubAppsServiceAddResultCode result_code) {
    return SubAppsServiceImpl::AddResultsToMojo(
        {{unhashed_app_id, result_code}});
  }

 protected:
  base::test::ScopedFeatureList features_{blink::features::kDesktopPWAsSubApps};
  AppId parent_app_id_;
  mojo::Remote<SubAppsService> remote_;
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

  std::string command = base::StringPrintf(
      R"(
        navigator.subApps.add({
            "%s": {"install_url": "%s"},
            "%s": {"install_url": "%s"},
        }))",
      unhashed_sub_app_id_1.c_str(), kSubAppUrl1.spec().c_str(),
      unhashed_sub_app_id_2.c_str(), kSubAppUrl2.spec().c_str());

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

  std::string command = base::StringPrintf(
      R"(
        navigator.subApps.add({
            "%s": {"install_url": "%s"},
            "%s": {"install_url": "%s"},
        }))",
      unhashed_sub_app_id.c_str(), kSubAppUrl.spec().c_str(),
      unhashed_invalid_sub_app_id.c_str(), kInvalidSubAppUrl.spec().c_str());

  // Add call promise should be rejected because an install failed.
  EXPECT_FALSE(ExecJs(render_frame_host(), command));
  EXPECT_EQ(1ul, GetAllSubAppIds(parent_app_id_).size());
}

// End-to-end. Test that adding a sub-app from a different origin or from a
// different domain fails.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest,
                       EndToEndAddFailDifferentOrigin) {
  NavigateToParentApp();
  InstallParentApp();
  EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id_).size());

  {
    GURL different_origin = https_server()->GetURL(kSubDomain, kSubAppPath);
    UnhashedAppId unhashed_sub_app_id =
        GenerateAppIdUnhashed(/*manifest_id=*/absl::nullopt, different_origin);

    std::string command = base::StringPrintf(
        R"(
        navigator.subApps.add({
            "%s": {"install_url": "%s"},
        }))",
        unhashed_sub_app_id.c_str(), different_origin.spec().c_str());

    // EXPECT_FALSE because this returns an error.
    EXPECT_FALSE(ExecJs(render_frame_host(), command));
    EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id_).size());
  }

  {
    GURL different_domain =
        https_server()->GetURL(kDifferentDomain, kSubAppPath2);
    UnhashedAppId unhashed_sub_app_id =
        GenerateAppIdUnhashed(/*manifest_id=*/absl::nullopt, different_domain);

    std::string command = base::StringPrintf(
        R"(
        navigator.subApps.add({
            "%s": {"install_url": "%s"},
        }))",
        unhashed_sub_app_id.c_str(), different_domain.spec().c_str());

    EXPECT_FALSE(ExecJs(render_frame_host(), command));
    EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id_).size());
  }
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

  EXPECT_EQ(AddResultMojo(unhashed_sub_app_id,
                          SubAppsServiceAddResultCode::kSuccessNewInstall),
            CallAdd({{unhashed_sub_app_id, kSubAppUrl}}));

  // Verify a bunch of things for the newly installed sub-app.
  EXPECT_TRUE(provider().registrar_unsafe().IsInstalled(sub_app_id));
  EXPECT_TRUE(provider().registrar_unsafe().IsLocallyInstalled(sub_app_id));
  EXPECT_EQ(
      DisplayMode::kStandalone,
      provider().registrar_unsafe().GetAppEffectiveDisplayMode(sub_app_id));

  const WebApp* sub_app = provider().registrar_unsafe().GetAppById(sub_app_id);
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
  EXPECT_EQ(AddResultMojo(unhashed_sub_app_id_1,
                          SubAppsServiceAddResultCode::kSuccessNewInstall),
            CallAdd({{unhashed_sub_app_id_1, kSubAppUrl1}}));
  EXPECT_EQ(1ul, GetAllSubAppIds(parent_app_id_).size());

  // Try to add first sub app again.
  EXPECT_EQ(
      AddResultMojo(unhashed_sub_app_id_1,
                    SubAppsServiceAddResultCode::kSuccessAlreadyInstalled),
      CallAdd({{unhashed_sub_app_id_1, kSubAppUrl1}}));
  EXPECT_EQ(1ul, GetAllSubAppIds(parent_app_id_).size());

  GURL kSubAppUrl2 = GetURL(kSubAppPath2);
  UnhashedAppId unhashed_sub_app_id_2 =
      GenerateAppIdUnhashed(/*manifest_id=*/absl::nullopt, kSubAppUrl2);

  // Add second sub app.
  EXPECT_EQ(AddResultMojo(unhashed_sub_app_id_2,
                          SubAppsServiceAddResultCode::kSuccessNewInstall),
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

  SubAppsServiceImpl::AddResults actual_results =
      AddResultsFromMojo(CallAdd(std::move(subapps)));

  EXPECT_THAT(actual_results,
              testing::UnorderedElementsAre(
                  std::pair{unhashed_sub_app_id_1,
                            SubAppsServiceAddResultCode::kSuccessNewInstall},
                  std::pair{unhashed_sub_app_id_2,
                            SubAppsServiceAddResultCode::kSuccessNewInstall},
                  std::pair{unhashed_sub_app_id_3,
                            SubAppsServiceAddResultCode::kSuccessNewInstall}));

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

  EXPECT_EQ(AddResultMojo(unhashed_sub_app_id,
                          SubAppsServiceAddResultCode::kSuccessNewInstall),
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

  SubAppsServiceImpl::AddResults actual_results =
      AddResultsFromMojo(CallAdd(std::move(subapps)));

  EXPECT_THAT(actual_results,
              testing::UnorderedElementsAre(
                  std::pair{unhashed_sub_app_id_1,
                            SubAppsServiceAddResultCode::kSuccessNewInstall},
                  std::pair{unhashed_sub_app_id_2,
                            SubAppsServiceAddResultCode::kInstallUrlInvalid},
                  std::pair{unhashed_sub_app_id_3,
                            SubAppsServiceAddResultCode::kSuccessNewInstall}));
  EXPECT_EQ(2ul, GetAllSubAppIds(parent_app_id_).size());
}

// Add call should fail if the parent app isn't installed.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest,
                       AddFailParentAppNotInstalled) {
  NavigateToParentApp();
  BindRemote();

  GURL kSubAppUrl = GetURL(kSubAppPath);
  UnhashedAppId unhashed_sub_app_id =
      GenerateAppIdUnhashed(/*manifest_id=*/absl::nullopt, kSubAppUrl);

  EXPECT_EQ(AddResultMojo(unhashed_sub_app_id,
                          SubAppsServiceAddResultCode::kParentAppUninstalled),
            CallAdd({{unhashed_sub_app_id, kSubAppUrl}}));
}

// TODO(isandrk): This test should probably live in SubAppInstallCommandTest
// (and this should even be impossible to happen with the locking mechanism we
// have on the command manager).
// Add call should fail if the parent app is uninstalled between the add call
// and the start of the command.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest,
                       AddFailParentAppWasUninstalled) {
  // Parent app installed.
  NavigateToParentApp();
  InstallParentApp();
  BindRemote();

  GURL kSubAppUrl = GetURL(kSubAppPath);
  UnhashedAppId unhashed_sub_app_id =
      GenerateAppIdUnhashed(/*manifest_id=*/absl::nullopt, kSubAppUrl);
  std::vector<std::pair<UnhashedAppId, GURL>> subapps = {
      {unhashed_sub_app_id, kSubAppUrl}};

  std::vector<SubAppsServiceAddInfoPtr> sub_apps_mojo;
  for (const auto& [unhashed_app_id, install_url] : subapps) {
    sub_apps_mojo.emplace_back(
        SubAppsServiceAddInfo::New(unhashed_app_id, install_url));
  }
  base::test::TestFuture<SubAppsServiceImpl::AddResultsMojo> future;

  // Add call made (sub app install command not started yet).
  remote_->Add(std::move(sub_apps_mojo), future.GetCallback());

  // Parent app uninstalled.
  UninstallParentApp();

  // Run sub app install command (does a RunLoop::Run() under the hood).
  SubAppsServiceImpl::AddResultsMojo actual = future.Take();

  SubAppsServiceImpl::AddResultsMojo expected = AddResultMojo(
      unhashed_sub_app_id, SubAppsServiceAddResultCode::kParentAppUninstalled);
  EXPECT_EQ(expected, actual);
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

  EXPECT_EQ(AddResultMojo(unhashed_sub_app_id,
                          SubAppsServiceAddResultCode::kParentAppUninstalled),
            CallAdd({{unhashed_sub_app_id, kSubAppUrl}}));
}

// Verify that Add fails for an empty list.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, AddEmptyList) {
  NavigateToParentApp();
  InstallParentApp();
  BindRemote();

  EXPECT_EQ(SubAppsServiceImpl::AddResultsMojo(), CallAdd({}));
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
      AddResultMojo(unhashed_sub_app_id,
                    SubAppsServiceAddResultCode::kExpectedAppIdCheckFailed),
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

  EXPECT_EQ(AddResultMojo(unhashed_sub_app_id,
                          SubAppsServiceAddResultCode::kInstallUrlInvalid),
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

  EXPECT_EQ(AddResultMojo(unhashed_sub_app_id_1,
                          SubAppsServiceAddResultCode::kSuccessNewInstall),
            CallAdd({{unhashed_sub_app_id_1, GetURL(kSubAppPath)}}));
  EXPECT_EQ(AddResultMojo(unhashed_sub_app_id_2,
                          SubAppsServiceAddResultCode::kSuccessNewInstall),
            CallAdd({{unhashed_sub_app_id_2, GetURL(kSubAppPath2)}}));
  EXPECT_EQ(AddResultMojo(unhashed_sub_app_id_3,
                          SubAppsServiceAddResultCode::kSuccessNewInstall),
            CallAdd({{unhashed_sub_app_id_3, GetURL(kSubAppPath3)}}));

  EXPECT_TRUE(provider().registrar_unsafe().IsInstalled(
      GenerateAppIdFromUnhashed(unhashed_sub_app_id_1)));
  EXPECT_TRUE(provider().registrar_unsafe().IsInstalled(
      GenerateAppIdFromUnhashed(unhashed_sub_app_id_2)));
  EXPECT_TRUE(provider().registrar_unsafe().IsInstalled(
      GenerateAppIdFromUnhashed(unhashed_sub_app_id_3)));

  UninstallParentApp();
  // Verify that both parent app and sub apps are no longer installed.
  EXPECT_FALSE(provider().registrar_unsafe().IsInstalled(parent_app_id_));
  EXPECT_FALSE(provider().registrar_unsafe().IsInstalled(
      GenerateAppIdFromUnhashed(unhashed_sub_app_id_1)));
  EXPECT_FALSE(provider().registrar_unsafe().IsInstalled(
      GenerateAppIdFromUnhashed(unhashed_sub_app_id_2)));
  EXPECT_FALSE(provider().registrar_unsafe().IsInstalled(
      GenerateAppIdFromUnhashed(unhashed_sub_app_id_3)));
}

// Verify that uninstalling an app that has multiple sources just
// removes a source and does not end up removing the sub_apps.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest,
                       RemovingSourceFromParentAppDoesNotRemoveSubApps) {
  NavigateToParentApp();
  InstallParentApp();
  BindRemote();

  // Add another source to mock installation from 2 sources.
  {
    ScopedRegistryUpdate update(&provider().sync_bridge());
    WebApp* web_app = update->UpdateApp(parent_app_id_);
    if (web_app)
      web_app->AddSource(WebAppManagement::kDefault);
  }

  // Verify that 2 subapps are installed.
  UnhashedAppId unhashed_sub_app_id_1 =
      GenerateAppIdUnhashed(/*manifest_id=*/absl::nullopt, GetURL(kSubAppPath));
  UnhashedAppId unhashed_sub_app_id_2 = GenerateAppIdUnhashed(
      /*manifest_id=*/absl::nullopt, GetURL(kSubAppPath2));

  EXPECT_EQ(AddResultMojo(unhashed_sub_app_id_1,
                          SubAppsServiceAddResultCode::kSuccessNewInstall),
            CallAdd({{unhashed_sub_app_id_1, GetURL(kSubAppPath)}}));
  EXPECT_EQ(AddResultMojo(unhashed_sub_app_id_2,
                          SubAppsServiceAddResultCode::kSuccessNewInstall),
            CallAdd({{unhashed_sub_app_id_2, GetURL(kSubAppPath2)}}));

  EXPECT_TRUE(provider().registrar_unsafe().IsInstalled(
      GenerateAppIdFromUnhashed(unhashed_sub_app_id_1)));
  EXPECT_TRUE(provider().registrar_unsafe().IsInstalled(
      GenerateAppIdFromUnhashed(unhashed_sub_app_id_2)));

  UninstallParentAppBySource(WebAppManagement::kDefault);
  // Verify that parent app and sub_apps are still installed, only
  // the default install source is removed from the parent app.
  EXPECT_TRUE(provider().registrar_unsafe().IsInstalled(parent_app_id_));
  EXPECT_FALSE(provider()
                   .registrar_unsafe()
                   .GetAppById(parent_app_id_)
                   ->IsPreinstalledApp());

  EXPECT_TRUE(provider().registrar_unsafe().IsInstalled(
      GenerateAppIdFromUnhashed(unhashed_sub_app_id_1)));
  EXPECT_TRUE(provider().registrar_unsafe().IsInstalled(
      GenerateAppIdFromUnhashed(unhashed_sub_app_id_2)));
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

  EXPECT_EQ(AddResultMojo(unhashed_sub_app_id,
                          SubAppsServiceAddResultCode::kSuccessNewInstall),
            CallAdd({{unhashed_sub_app_id, kSubAppUrl}}));
  EXPECT_EQ(DisplayMode::kStandalone,
            provider().registrar_unsafe().GetAppEffectiveDisplayMode(
                GenerateAppIdFromUnhashed(unhashed_sub_app_id)));

  GURL kSubAppWithMinialUiUrl = GetURL(kSubAppPathMinimalUi);

  EXPECT_EQ(
      AddResultMojo(unhashed_sub_app_id,
                    SubAppsServiceAddResultCode::kSuccessAlreadyInstalled),
      CallAdd({{unhashed_sub_app_id, kSubAppWithMinialUiUrl}}));
  EXPECT_EQ(DisplayMode::kStandalone,
            provider().registrar_unsafe().GetAppEffectiveDisplayMode(
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
  EXPECT_EQ(AddResultMojo(unhashed_sub_app_id,
                          SubAppsServiceAddResultCode::kSuccessNewInstall),
            CallAdd({{unhashed_sub_app_id, GetURL(kSubAppPath)}}));

  EXPECT_TRUE(provider().registrar_unsafe().IsInstalled(
      GenerateAppIdFromUnhashed(unhashed_sub_app_id)));

  // Add standalone app as sub-app.
  const WebApp* standalone_app =
      provider().registrar_unsafe().GetAppById(standalone_app_id);
  EXPECT_EQ(AddResultMojo(unhashed_standalone_app_id,
                          SubAppsServiceAddResultCode::kSuccessNewInstall),
            CallAdd({{unhashed_standalone_app_id, GetURL(kSubAppPath2)}}));

  // Verify that it is now installed and registered as a subapp.

  EXPECT_EQ(parent_app_id_, standalone_app->parent_app_id());
  EXPECT_FALSE(standalone_app->HasOnlySource(WebAppManagement::kSync));
  EXPECT_TRUE(standalone_app->IsSubAppInstalledApp());

  UninstallParentApp();

  // Verify that normal sub-app is uninstalled.
  EXPECT_FALSE(provider().registrar_unsafe().IsInstalled(
      GenerateAppIdFromUnhashed(unhashed_sub_app_id)));

  // Verify that previous standalone is still installed.
  EXPECT_TRUE(provider().registrar_unsafe().IsInstalled(standalone_app_id));

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
  EXPECT_EQ(std::vector<SubAppsServiceListInfoPtr>{}, result->sub_apps);

  UnhashedAppId unhashed_sub_app_id_1 =
      GenerateAppIdUnhashed(/*manifest_id=*/absl::nullopt, GetURL(kSubAppPath));
  UnhashedAppId unhashed_sub_app_id_2 = GenerateAppIdUnhashed(
      /*manifest_id=*/absl::nullopt, GetURL(kSubAppPath2));
  UnhashedAppId unhashed_sub_app_id_3 = GenerateAppIdUnhashed(
      /*manifest_id=*/absl::nullopt, GetURL(kSubAppPath3));

  EXPECT_EQ(AddResultMojo(unhashed_sub_app_id_1,
                          SubAppsServiceAddResultCode::kSuccessNewInstall),
            CallAdd({{unhashed_sub_app_id_1, GetURL(kSubAppPath)}}));
  EXPECT_EQ(AddResultMojo(unhashed_sub_app_id_2,
                          SubAppsServiceAddResultCode::kSuccessNewInstall),
            CallAdd({{unhashed_sub_app_id_2, GetURL(kSubAppPath2)}}));
  EXPECT_EQ(AddResultMojo(unhashed_sub_app_id_3,
                          SubAppsServiceAddResultCode::kSuccessNewInstall),
            CallAdd({{unhashed_sub_app_id_3, GetURL(kSubAppPath3)}}));

  // We need to use a set for comparison because the ordering changes between
  // invocations (due to embedded test server using a random port each time).
  base::flat_set<SubAppsServiceListInfoPtr> expected_set;
  expected_set.emplace(
      SubAppsServiceListInfo::New(unhashed_sub_app_id_1, kSubAppName));
  expected_set.emplace(
      SubAppsServiceListInfo::New(unhashed_sub_app_id_2, kSubAppName2));
  expected_set.emplace(
      SubAppsServiceListInfo::New(unhashed_sub_app_id_3, kSubAppName3));

  result = CallList();

  // We see all three sub-apps now. We need to use UnorderedElementsAre because
  // the ordering changes between invocations (due to embedded test server using
  // a random port each time).
  EXPECT_EQ(SubAppsServiceResult::kSuccess, result->code);
  base::flat_set<SubAppsServiceListInfoPtr> actual_set(
      std::make_move_iterator(result->sub_apps.begin()),
      std::make_move_iterator(result->sub_apps.end()));
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
  EXPECT_EQ(AddResultMojo(
                unhashed_sub_app_id_2,
                blink::mojom::SubAppsServiceAddResultCode::kSuccessNewInstall),
            CallAdd({{unhashed_sub_app_id_2, GetURL(kSubAppPath2)}}));

  std::vector<SubAppsServiceListInfoPtr> expected_result;
  expected_result.emplace_back(
      SubAppsServiceListInfo::New(unhashed_sub_app_id_2, kSubAppName2));

  // Should only see the sub-app one here, not the standalone.
  SubAppsServiceListResultPtr result = CallList();
  EXPECT_EQ(SubAppsServiceResult::kSuccess, result->code);
  EXPECT_EQ(expected_result, result->sub_apps);
}

// List call returns failure if the parent app isn't installed.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest,
                       ListFailParentAppNotInstalled) {
  NavigateToParentApp();
  BindRemote();

  SubAppsServiceListResultPtr result = CallList();
  EXPECT_EQ(SubAppsServiceResult::kFailure, result->code);
  EXPECT_EQ(std::vector<SubAppsServiceListInfoPtr>{}, result->sub_apps);
}

// Remove works with one app.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, RemoveOneApp) {
  InstallParentApp();
  NavigateToParentApp();
  BindRemote();

  UnhashedAppId unhashed_app_id = GetURL(kSubAppPath).spec();
  AppId app_id = GenerateAppIdFromUnhashed(unhashed_app_id);

  EXPECT_EQ(AddResultMojo(unhashed_app_id,
                          SubAppsServiceAddResultCode::kSuccessNewInstall),
            CallAdd({{unhashed_app_id, GetURL(kSubAppPath)}}));
  EXPECT_EQ(1ul, GetAllSubAppIds(parent_app_id_).size());
  EXPECT_TRUE(provider().registrar_unsafe().IsInstalled(app_id));

  EXPECT_EQ(SubAppsServiceResult::kSuccess, CallRemove(unhashed_app_id));
  EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id_).size());
  EXPECT_FALSE(provider().registrar_unsafe().IsInstalled(app_id));
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

  EXPECT_EQ(AddResultMojo(unhashed_sub_app_id,
                          SubAppsServiceAddResultCode::kSuccessNewInstall),
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
