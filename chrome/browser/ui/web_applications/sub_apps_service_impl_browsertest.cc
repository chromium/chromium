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
using blink::mojom::SubAppsServiceListInfo;
using blink::mojom::SubAppsServiceListInfoPtr;
using blink::mojom::SubAppsServiceListResultPtr;
using blink::mojom::SubAppsServiceResult;

namespace web_app {

namespace {

// `kDomain` must be just a hostname, not a full URL.
constexpr const char kDomain[] = "www.foo.bar";
constexpr const char kDifferentDomain[] = "https://www.different-domain.com/";
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
constexpr const char kSubAppIdInvalid[] = "/invalid-sub-app-id";

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

  GURL GetURLFromPath(const std::string& path) {
    return https_server()->GetURL(kDomain, path);
  }

  AppId GenerateAppIdFromPath(const std::string& path) {
    return GenerateAppId(/*manifest_id=*/absl::nullopt, GetURLFromPath(path));
  }

  AppId InstallPWAFromPath(const std::string& path) {
    return InstallPWA(GetURLFromPath(path));
  }

  void InstallParentApp() {
    parent_app_id_ = InstallPWAFromPath(kParentAppPath);
  }

  void NavigateToPath(const std::string& path) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetURLFromPath(path)));
  }

  void NavigateToParentApp() { NavigateToPath(kParentAppPath); }

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
  // it to finish. Argument should contain paths, not full URLs.
  SubAppsServiceImpl::AddResults CallAdd(
      std::vector<std::pair<std::string, std::string>> subapps) {
    // Convert params to mojo before making the call.
    std::vector<SubAppsServiceAddInfoPtr> sub_apps_mojo;
    for (const auto& [unhashed_app_id_path, install_url_path] : subapps) {
      sub_apps_mojo.emplace_back(
          SubAppsServiceAddInfo::New(unhashed_app_id_path, install_url_path));
    }

    base::test::TestFuture<SubAppsServiceImpl::AddResultsMojo> future;
    remote_->Add(std::move(sub_apps_mojo), future.GetCallback());

    // Unpack the mojo results before returning them.
    SubAppsServiceImpl::AddResults add_results;
    for (const auto& result : future.Take()) {
      add_results.emplace_back(result->unhashed_app_id_path,
                               result->result_code);
    }
    return add_results;
  }

  void ExpectCallAdd(
      base::flat_set<std::pair<std::string, SubAppsServiceResult>> expected,
      std::vector<std::pair<std::string, std::string>> subapps) {
    SubAppsServiceImpl::AddResults actual = CallAdd(subapps);
    // We need to use a set for comparison because the ordering changes between
    // invocations (due to embedded test server using a random port each time).
    base::flat_set<std::pair<UnhashedAppId, blink::mojom::SubAppsServiceResult>>
        actual_set{actual};
    EXPECT_EQ(expected, actual_set);
  }

  // Calls the List() method on the mojo interface which is async, and waits for
  // it to finish.
  SubAppsServiceListResultPtr CallList() {
    base::test::TestFuture<SubAppsServiceListResultPtr> future;
    remote_->List(future.GetCallback());
    return future.Take();
  }

  // Calls the Remove() method on the mojo interface which is async, and waits
  // for it to finish. Argument should be a path, not a full URL.
  SubAppsServiceResult CallRemove(const std::string& unhashed_app_id_path) {
    base::test::TestFuture<SubAppsServiceResult> future;
    remote_->Remove(unhashed_app_id_path, future.GetCallback());
    return future.Get();
  }

 protected:
  base::test::ScopedFeatureList features_{blink::features::kDesktopPWAsSubApps};
  AppId parent_app_id_;
  mojo::Remote<SubAppsService> remote_;
};

/********** End-to-end test (one is enough!). **********/

// Simple end-to-end test for add().
// NOTE: Only one E2E test is enough, test everything else through the Mojo
// interface (as all the other tests do).
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, EndToEndAdd) {
  NavigateToParentApp();
  InstallParentApp();
  EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id_).size());

  std::string command = base::StringPrintf(
      R"(
        navigator.subApps.add({
            "%s": {"install_url": "%s"},
            "%s": {"install_url": "%s"},
        }))",
      kSubAppPath, kSubAppPath, kSubAppPath2, kSubAppPath2);

  EXPECT_TRUE(ExecJs(render_frame_host(), command));
  EXPECT_EQ(2ul, GetAllSubAppIds(parent_app_id_).size());
}

/********** Tests for the Add API call. **********/

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

  ExpectCallAdd({{kSubAppPath, SubAppsServiceResult::kSuccess}},
                {{kSubAppPath, kSubAppPath}});

  // Verify a bunch of things for the newly installed sub-app.
  AppId sub_app_id = GenerateAppIdFromPath(kSubAppPath);
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
  EXPECT_EQ(GetURLFromPath(kSubAppPath), sub_app->start_url());
  if (provider().ui_manager().CanAddAppToQuickLaunchBar()) {
    EXPECT_FALSE(provider().ui_manager().IsAppInQuickLaunchBar(sub_app_id));
  }
}

// Verify that Add works if PWA is launched as standalone window.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, AddStandaloneWindow) {
  NavigateToParentApp();
  InstallParentApp();
  content::WebContents* web_contents = OpenApplication(parent_app_id_);
  BindRemote(web_contents);

  ExpectCallAdd({{kSubAppPath, SubAppsServiceResult::kSuccess}},
                {{kSubAppPath, kSubAppPath}});
}

// Add call should fail if the parent app isn't installed.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest,
                       AddFailParentAppNotInstalled) {
  NavigateToParentApp();
  BindRemote();

  ExpectCallAdd({{kSubAppPath, SubAppsServiceResult::kFailure}},
                {{kSubAppPath, kSubAppPath}});
}

// Add call should fail if the call wasn't made from the context of parent app.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest,
                       AddFailNotInParentAppContext) {
  InstallParentApp();
  BindRemote();

  ExpectCallAdd({{kSubAppPath, SubAppsServiceResult::kFailure}},
                {{kSubAppPath, kSubAppPath}});
}

// Verify that Add call rejects a sub-app with the wrong specified app_id.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, AddFailIncorrectId) {
  NavigateToParentApp();
  InstallParentApp();
  BindRemote();

  ExpectCallAdd({{kSubAppIdInvalid, SubAppsServiceResult::kFailure}},
                {{kSubAppIdInvalid, kSubAppPath}});
  EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id_).size());
}

// Verify that Add fails for an invalid (non-existing) sub-app.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, AddFailNonExistent) {
  NavigateToParentApp();
  InstallParentApp();
  BindRemote();

  ExpectCallAdd({{kSubAppPathInvalid, SubAppsServiceResult::kFailure}},
                {{kSubAppPathInvalid, kSubAppPathInvalid}});
  EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id_).size());
}

// Add call closes the mojo connection if the argument is wrong origin to the
// calling app.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, AddFailWrongOrigin) {
  InstallParentApp();
  NavigateToParentApp();
  BindRemote();

  base::RunLoop run_loop;
  remote_.set_disconnect_handler(
      base::BindLambdaForTesting([&]() { run_loop.Quit(); }));
  // This call should never succeed and the disconnect handler should be called
  // instead.
  std::vector<SubAppsServiceAddInfoPtr> sub_apps_mojo;
  sub_apps_mojo.emplace_back(
      SubAppsServiceAddInfo::New(kDifferentDomain, kDifferentDomain));
  remote_->Add(std::move(sub_apps_mojo),
               base::BindLambdaForTesting(
                   [](SubAppsServiceImpl::AddResultsMojo results) {
                     ADD_FAILURE() << "Callback unexpectedly invoked.";
                   }));
  run_loop.Run();
}

// Make sure the Add API can't force manifest update. Add sub-app, verify
// display mode, then add the same one again with different display mode in the
// manifest, and verify that it didn't change.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, AddDoesntForceReinstall) {
  NavigateToParentApp();
  InstallParentApp();
  BindRemote();

  ExpectCallAdd({{kSubAppPath, SubAppsServiceResult::kSuccess}},
                {{kSubAppPath, kSubAppPath}});
  AppId sub_app_id = GenerateAppIdFromPath(kSubAppPath);
  EXPECT_EQ(
      DisplayMode::kStandalone,
      provider().registrar_unsafe().GetAppEffectiveDisplayMode(sub_app_id));

  ExpectCallAdd({{kSubAppPath, SubAppsServiceResult::kSuccess}},
                {{kSubAppPath, kSubAppPathMinimalUi}});
  EXPECT_EQ(
      DisplayMode::kStandalone,
      provider().registrar_unsafe().GetAppEffectiveDisplayMode(sub_app_id));
}

/******** Tests for the Add API call - adding multiple/zero sub-apps. ********/

// Add one sub-app, verify count is one. Add it again, still same count. Add a
// second sub-app, verify count is two.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, AddTwo) {
  NavigateToParentApp();
  InstallParentApp();

  BindRemote();

  EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id_).size());

  ExpectCallAdd({{kSubAppPath, SubAppsServiceResult::kSuccess}},
                {{kSubAppPath, kSubAppPath}});
  EXPECT_EQ(1ul, GetAllSubAppIds(parent_app_id_).size());

  // Try to add first sub app again.
  ExpectCallAdd({{kSubAppPath, SubAppsServiceResult::kSuccess}},
                {{kSubAppPath, kSubAppPath}});
  EXPECT_EQ(1ul, GetAllSubAppIds(parent_app_id_).size());

  // Add second sub app.
  ExpectCallAdd({{kSubAppPath2, SubAppsServiceResult::kSuccess}},
                {{kSubAppPath2, kSubAppPath2}});
  EXPECT_EQ(2ul, GetAllSubAppIds(parent_app_id_).size());
}

// Verify that Adding multiple sub-apps works correctly.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, AddMultiple) {
  NavigateToParentApp();
  InstallParentApp();
  BindRemote();

  ExpectCallAdd({{kSubAppPath, SubAppsServiceResult::kSuccess},
                 {kSubAppPath2, SubAppsServiceResult::kSuccess},
                 {kSubAppPath3, SubAppsServiceResult::kSuccess}},
                {{kSubAppPath, kSubAppPath},
                 {kSubAppPath2, kSubAppPath2},
                 {kSubAppPath3, kSubAppPath3}});

  EXPECT_EQ(3ul, GetAllSubAppIds(parent_app_id_).size());
}

// Verify that Adding a mix of valid and invalid sub-apps works.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest,
                       AddMultipleWithInvalidSubApps) {
  NavigateToParentApp();
  InstallParentApp();
  BindRemote();

  ExpectCallAdd({{kSubAppPath, SubAppsServiceResult::kSuccess},
                 {kSubAppPathInvalid, SubAppsServiceResult::kFailure},
                 {kSubAppPath3, SubAppsServiceResult::kSuccess}},
                {{kSubAppPath, kSubAppPath},
                 {kSubAppPathInvalid, kSubAppPathInvalid},
                 {kSubAppPath3, kSubAppPath3}});
  EXPECT_EQ(2ul, GetAllSubAppIds(parent_app_id_).size());
}

// Verify that Add works correctly for 0 sub-apps to be installed.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, AddZero) {
  NavigateToParentApp();
  InstallParentApp();
  BindRemote();

  ExpectCallAdd({}, {});
  EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id_).size());
}

/********** Tests for uninstallation behaviour. **********/

// Verify that uninstalling an app with sub-apps causes sub-apps to be
// uninstalled as well.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest,
                       UninstallingParentAppUninstallsSubApps) {
  NavigateToParentApp();
  InstallParentApp();
  BindRemote();

  ExpectCallAdd({{kSubAppPath, SubAppsServiceResult::kSuccess},
                 {kSubAppPath2, SubAppsServiceResult::kSuccess},
                 {kSubAppPath3, SubAppsServiceResult::kSuccess}},
                {{kSubAppPath, kSubAppPath},
                 {kSubAppPath2, kSubAppPath2},
                 {kSubAppPath3, kSubAppPath3}});

  // Verify that sub-apps are installed.
  AppId sub_app_id_1 = GenerateAppIdFromPath(kSubAppPath);
  AppId sub_app_id_2 = GenerateAppIdFromPath(kSubAppPath2);
  AppId sub_app_id_3 = GenerateAppIdFromPath(kSubAppPath3);

  EXPECT_TRUE(provider().registrar_unsafe().IsInstalled(sub_app_id_1));
  EXPECT_TRUE(provider().registrar_unsafe().IsInstalled(sub_app_id_2));
  EXPECT_TRUE(provider().registrar_unsafe().IsInstalled(sub_app_id_3));

  UninstallParentApp();

  // Verify that both parent app and sub-apps are no longer installed.
  EXPECT_FALSE(provider().registrar_unsafe().IsInstalled(parent_app_id_));
  EXPECT_FALSE(provider().registrar_unsafe().IsInstalled(sub_app_id_1));
  EXPECT_FALSE(provider().registrar_unsafe().IsInstalled(sub_app_id_2));
  EXPECT_FALSE(provider().registrar_unsafe().IsInstalled(sub_app_id_3));
}

// Verify that uninstalling one source of the parent app which has multiple
// sources of installation doesn't actually uninstall it (or the sub-apps it has
// added).
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest,
                       UninstallingParentAppSourceDoesntUninstallSubApps) {
  NavigateToParentApp();
  InstallParentApp();
  BindRemote();

  // Add another source to the parent app.
  {
    ScopedRegistryUpdate update(&provider().sync_bridge_unsafe());
    WebApp* web_app = update->UpdateApp(parent_app_id_);
    ASSERT_TRUE(web_app);
    web_app->AddSource(WebAppManagement::kDefault);
  }

  AppId sub_app_id_1 = GenerateAppIdFromPath(kSubAppPath);
  AppId sub_app_id_2 = GenerateAppIdFromPath(kSubAppPath2);

  ExpectCallAdd({{kSubAppPath, SubAppsServiceResult::kSuccess},
                 {kSubAppPath2, SubAppsServiceResult::kSuccess}},
                {{kSubAppPath, kSubAppPath}, {kSubAppPath2, kSubAppPath2}});

  // Verify that 2 sub-apps are installed.
  EXPECT_TRUE(provider().registrar_unsafe().IsInstalled(sub_app_id_1));
  EXPECT_TRUE(provider().registrar_unsafe().IsInstalled(sub_app_id_2));

  UninstallParentAppBySource(WebAppManagement::kDefault);

  // Verify that the parent app and the sub-apps are still installed, only the
  // default install source is removed from the parent app.
  EXPECT_TRUE(provider().registrar_unsafe().IsInstalled(parent_app_id_));
  EXPECT_FALSE(provider()
                   .registrar_unsafe()
                   .GetAppById(parent_app_id_)
                   ->IsPreinstalledApp());
  EXPECT_TRUE(provider().registrar_unsafe().IsInstalled(sub_app_id_1));
  EXPECT_TRUE(provider().registrar_unsafe().IsInstalled(sub_app_id_2));
}

// Verify that uninstalling an app that has a sub-app with more than one install
// source only removes the "sub-app" install source for that sub-app but does
// not uninstall it.
IN_PROC_BROWSER_TEST_F(
    SubAppsServiceImplBrowserTest,
    UninstallingParentAppDoesntUninstallSubAppWithMultipleSources) {
  // Install app as standalone app.
  AppId standalone_app_id = InstallPWAFromPath(kSubAppPath2);

  NavigateToParentApp();
  InstallParentApp();
  BindRemote();

  // Add another sub-app to verify standalone app install/uninstall does not
  // affect normal sub-app uninstalls.
  ExpectCallAdd({{kSubAppPath, SubAppsServiceResult::kSuccess}},
                {{kSubAppPath, kSubAppPath}});
  AppId sub_app_id = GenerateAppIdFromPath(kSubAppPath);
  EXPECT_TRUE(provider().registrar_unsafe().IsInstalled(sub_app_id));

  // Add standalone app as sub-app.
  const WebApp* standalone_app =
      provider().registrar_unsafe().GetAppById(standalone_app_id);
  ExpectCallAdd({{kSubAppPath2, SubAppsServiceResult::kSuccess}},
                {{kSubAppPath2, kSubAppPath2}});

  // Verify that it is now installed and registered as a sub-app.
  EXPECT_EQ(parent_app_id_, standalone_app->parent_app_id());
  EXPECT_FALSE(standalone_app->HasOnlySource(WebAppManagement::kSync));
  EXPECT_TRUE(standalone_app->IsSubAppInstalledApp());

  UninstallParentApp();

  // Verify that the second sub-app is uninstalled.
  EXPECT_FALSE(provider().registrar_unsafe().IsInstalled(sub_app_id));

  // Verify that previous standalone is still installed.
  EXPECT_TRUE(provider().registrar_unsafe().IsInstalled(standalone_app_id));

  // Verify that there are no apps registered as parent app's sub apps.
  EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id_).size());
  EXPECT_EQ(absl::nullopt, standalone_app->parent_app_id());

  // Verify that the standalone app no longer has the sub-app install source.
  EXPECT_TRUE(standalone_app->HasOnlySource(WebAppManagement::kSync));
}

/********** Tests for the List API call. **********/

// List call returns the correct value for three sub-apps.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, ListSuccess) {
  NavigateToParentApp();
  InstallParentApp();
  BindRemote();

  // Empty list before adding any sub-apps.
  SubAppsServiceListResultPtr result = CallList();
  EXPECT_EQ(SubAppsServiceResult::kSuccess, result->code);
  EXPECT_EQ(std::vector<SubAppsServiceListInfoPtr>{}, result->sub_apps_list);

  ExpectCallAdd({{kSubAppPath, SubAppsServiceResult::kSuccess},
                 {kSubAppPath2, SubAppsServiceResult::kSuccess},
                 {kSubAppPath3, SubAppsServiceResult::kSuccess}},
                {{kSubAppPath, kSubAppPath},
                 {kSubAppPath2, kSubAppPath2},
                 {kSubAppPath3, kSubAppPath3}});

  result = CallList();

  // We need to use a set for comparison because the ordering changes between
  // invocations (due to embedded test server using a random port each time).
  base::flat_set<SubAppsServiceListInfoPtr> expected_set;
  expected_set.emplace(SubAppsServiceListInfo::New(kSubAppPath, kSubAppName));
  expected_set.emplace(SubAppsServiceListInfo::New(kSubAppPath2, kSubAppName2));
  expected_set.emplace(SubAppsServiceListInfo::New(kSubAppPath3, kSubAppName3));

  base::flat_set<SubAppsServiceListInfoPtr> actual_set(
      std::make_move_iterator(result->sub_apps_list.begin()),
      std::make_move_iterator(result->sub_apps_list.end()));

  // We see all three sub-apps now.
  EXPECT_EQ(SubAppsServiceResult::kSuccess, result->code);
  EXPECT_EQ(expected_set, actual_set);
}

// Verify that the list call doesn't return a non-sub-apps installed app.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest,
                       ListDoesntReturnNonSubApp) {
  // Regular install.
  InstallPWAFromPath(kSubAppPath);

  NavigateToParentApp();
  InstallParentApp();
  BindRemote();

  // Sub-app install.
  ExpectCallAdd({{kSubAppPath2, SubAppsServiceResult::kSuccess}},
                {{kSubAppPath2, kSubAppPath2}});

  std::vector<SubAppsServiceListInfoPtr> expected_result;
  expected_result.emplace_back(
      SubAppsServiceListInfo::New(kSubAppPath2, kSubAppName2));

  // Should only see the sub-app one here, not the standalone.
  SubAppsServiceListResultPtr result = CallList();
  EXPECT_EQ(SubAppsServiceResult::kSuccess, result->code);
  EXPECT_EQ(expected_result, result->sub_apps_list);
}

// List call returns failure if the parent app isn't installed.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest,
                       ListFailParentAppNotInstalled) {
  NavigateToParentApp();
  BindRemote();

  SubAppsServiceListResultPtr result = CallList();
  EXPECT_EQ(SubAppsServiceResult::kFailure, result->code);
  EXPECT_EQ(std::vector<SubAppsServiceListInfoPtr>{}, result->sub_apps_list);
}

/********** Tests for the Remove API call. **********/

// Remove works with one app.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, RemoveOneApp) {
  InstallParentApp();
  NavigateToParentApp();
  BindRemote();

  ExpectCallAdd({{kSubAppPath, SubAppsServiceResult::kSuccess}},
                {{kSubAppPath, kSubAppPath}});

  AppId app_id = GenerateAppIdFromPath(kSubAppPath);
  EXPECT_EQ(1ul, GetAllSubAppIds(parent_app_id_).size());
  EXPECT_TRUE(provider().registrar_unsafe().IsInstalled(app_id));

  EXPECT_EQ(SubAppsServiceResult::kSuccess, CallRemove(kSubAppPath));
  EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id_).size());
  EXPECT_FALSE(provider().registrar_unsafe().IsInstalled(app_id));
}

// Remove fails for a regular installed app.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, RemoveFailRegularApp) {
  // Regular install.
  InstallPWAFromPath(kSubAppPath);

  InstallParentApp();
  NavigateToParentApp();
  BindRemote();

  EXPECT_EQ(SubAppsServiceResult::kFailure, CallRemove(kSubAppPath));
}

// Remove fails for a sub-app with a different parent_app_id.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, RemoveFailWrongParent) {
  // SubApp plays the parent app here, SubApp2 is its sub-app, SubApp3 is the
  // other "parent app".
  AppId parent_app = InstallPWAFromPath(kSubAppPath);
  NavigateToPath(kSubAppPath);
  BindRemote();

  ExpectCallAdd({{kSubAppPath2, SubAppsServiceResult::kSuccess}},
                {{kSubAppPath2, kSubAppPath2}});

  AppId second_parent_app = InstallPWAFromPath(kSubAppPath3);
  NavigateToPath(kSubAppPath3);
  remote_.reset();
  BindRemote();

  EXPECT_EQ(SubAppsServiceResult::kFailure, CallRemove(kSubAppPath2));
}

// Remove call returns failure if the calling app isn't installed.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest,
                       RemoveFailCallingAppNotInstalled) {
  NavigateToParentApp();
  BindRemote();

  EXPECT_EQ(SubAppsServiceResult::kFailure, CallRemove(kSubAppPath));
}

// Remove call closes the mojo connection if the argument is wrong origin to the
// calling app.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, RemoveFailWrongOrigin) {
  InstallParentApp();
  NavigateToParentApp();
  BindRemote();

  base::RunLoop run_loop;
  remote_.set_disconnect_handler(
      base::BindLambdaForTesting([&]() { run_loop.Quit(); }));
  // This call should never succeed and the disconnect handler should be called
  // instead.
  remote_->Remove(kDifferentDomain,
                  base::BindLambdaForTesting([](SubAppsServiceResult result) {
                    ADD_FAILURE() << "Callback unexpectedly invoked.";
                  }));
  run_loop.Run();
}

}  // namespace web_app
