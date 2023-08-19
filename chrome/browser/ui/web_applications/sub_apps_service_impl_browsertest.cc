// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>
#include <vector>

#include "chrome/browser/ui/web_applications/sub_apps_service_impl.h"

#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/sub_apps_install_dialog_controller.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

using blink::mojom::SubAppsService;
using blink::mojom::SubAppsServiceAddParameters;
using blink::mojom::SubAppsServiceAddParametersPtr;
using blink::mojom::SubAppsServiceListResultEntry;
using blink::mojom::SubAppsServiceListResultEntryPtr;
using blink::mojom::SubAppsServiceListResultPtr;
using blink::mojom::SubAppsServiceRemoveResult;
using blink::mojom::SubAppsServiceRemoveResultPtr;
using blink::mojom::SubAppsServiceResultCode;

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

using RemoveResultsMojo =
    std::vector<blink::mojom::SubAppsServiceRemoveResultPtr>;

using AddResults =
    std::vector<std::pair<ManifestId, blink::mojom::SubAppsServiceResultCode>>;

// There's one simple end-to-end test that actually calls the JS API interface,
// the rest test the mojo interface (since the first layer listening to the API
// calls is almost a direct passthrough to the mojo service).
//
// JS API interface tests are in
// third_party/blink/web_tests/external/wpt/subapps/.

class SubAppsServiceImplBrowserTest : public WebAppControllerBrowserTest {
 public:
  SubAppsServiceImplBrowserTest()
      : dialog_override_(
            SubAppsInstallDialogController::SetAutomaticActionForTesting(
                SubAppsInstallDialogController::DialogActionForTesting::
                    kAccept)) {}
  void SetUpOnMainThread() override {
    WebAppControllerBrowserTest::SetUpOnMainThread();
    notification_display_service_ =
        std::make_unique<NotificationDisplayServiceTester>(profile());
  }

  void TearDownOnMainThread() override {
    notification_display_service_.reset();
    WebAppControllerBrowserTest::TearDownOnMainThread();
  }

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
    base::test::TestFuture<void> uninstall_future;
    provider().scheduler().RemoveInstallSource(
        parent_app_id_, source, webapps::WebappUninstallSource::kAppsPage,
        base::BindLambdaForTesting([&](webapps::UninstallResultCode code) {
          EXPECT_EQ(code, webapps::UninstallResultCode::kSuccess);
          uninstall_future.SetValue();
        }));
    ASSERT_TRUE(uninstall_future.Wait())
        << "UninstallExternalWebApp did not trigger the callback.";
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
  AddResults CallAdd(std::vector<std::pair<std::string, std::string>> subapps) {
    // Convert params to mojo before making the call.
    std::vector<SubAppsServiceAddParametersPtr> sub_apps_mojo;
    for (const auto& [manifest_id_path, install_url_path] : subapps) {
      sub_apps_mojo.emplace_back(
          SubAppsServiceAddParameters::New(manifest_id_path, install_url_path));
    }

    base::test::TestFuture<SubAppsServiceImpl::AddResultsMojo> future;
    remote_->Add(std::move(sub_apps_mojo), future.GetCallback());
    EXPECT_TRUE(future.Wait()) << "Add did not trigger the callback.";

    // Unpack the mojo results before returning them.
    AddResults add_results;
    for (const auto& result : future.Take()) {
      add_results.emplace_back(GetURLFromPath(result->manifest_id_path),
                               result->result_code);
    }
    return add_results;
  }

  void ExpectCallAdd(
      base::flat_set<std::pair<ManifestId, SubAppsServiceResultCode>> expected,
      std::vector<std::pair<std::string, std::string>> subapps) {
    AddResults actual = CallAdd(subapps);
    EXPECT_THAT(actual, testing::UnorderedElementsAreArray(expected));
  }

  // Calls the List() method on the mojo interface which is async, and waits for
  // it to finish.
  SubAppsServiceListResultPtr CallList() {
    base::test::TestFuture<SubAppsServiceListResultPtr> future;
    remote_->List(future.GetCallback());
    EXPECT_TRUE(future.Wait()) << "List did not trigger the callback.";
    return future.Take();
  }

  // Calls the Remove() method on the mojo interface which is async, and waits
  // for it to finish.
  RemoveResultsMojo CallRemove(
      const std::vector<std::string>& manifest_id_paths) {
    base::test::TestFuture<RemoveResultsMojo> future;
    remote_->Remove(manifest_id_paths, future.GetCallback());
    EXPECT_TRUE(future.Wait()) << "Remove did not trigger the callback.";
    return future.Take();
  }

  RemoveResultsMojo SingleRemoveResultMojo(
      const std::string& manifest_id_path,
      SubAppsServiceResultCode result_code) {
    std::vector<blink::mojom::SubAppsServiceRemoveResultPtr> result;
    result.emplace_back(
        SubAppsServiceRemoveResult::New(manifest_id_path, result_code));
    return result;
  }

  std::vector<std::pair<ManifestId, SubAppsServiceResultCode>>
  RemoveResultsToList(RemoveResultsMojo results) {
    std::vector<std::pair<ManifestId, SubAppsServiceResultCode>> list;
    for (auto& result : results) {
      list.emplace_back(GetURLFromPath(result->manifest_id_path),
                        result->result_code);
    }
    return list;
  }

  bool UninstallNotificationShown() {
    return notification_display_service_
        ->GetNotification(SubAppsServiceImpl::kSubAppsUninstallNotificationId)
        .has_value();
  }

 protected:
  base::test::ScopedFeatureList features_{blink::features::kDesktopPWAsSubApps};
  AppId parent_app_id_;
  mojo::Remote<SubAppsService> remote_;
  base::AutoReset<
      absl::optional<SubAppsInstallDialogController::DialogActionForTesting>>
      dialog_override_;
  std::unique_ptr<NotificationDisplayServiceTester>
      notification_display_service_;
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
            "%s": {"installURL": "%s"},
            "%s": {"installURL": "%s"},
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

  ExpectCallAdd(
      {{GetURLFromPath(kSubAppPath), SubAppsServiceResultCode::kSuccess}},
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

  ExpectCallAdd(
      {{GetURLFromPath(kSubAppPath), SubAppsServiceResultCode::kSuccess}},
      {{kSubAppPath, kSubAppPath}});
}

// Add call should fail if the parent app isn't installed.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest,
                       AddFailParentAppNotInstalled) {
  NavigateToParentApp();
  BindRemote();

  ExpectCallAdd(
      {{GetURLFromPath(kSubAppPath), SubAppsServiceResultCode::kFailure}},
      {{kSubAppPath, kSubAppPath}});
}

// Add call should fail if the call wasn't made from the context of parent app.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest,
                       AddFailNotInParentAppContext) {
  InstallParentApp();
  BindRemote();

  ExpectCallAdd(
      {{GetURLFromPath(kSubAppPath), SubAppsServiceResultCode::kFailure}},
      {{kSubAppPath, kSubAppPath}});
}

// Verify that Add call rejects a sub-app with the wrong specified app_id.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, AddFailIncorrectId) {
  NavigateToParentApp();
  InstallParentApp();
  BindRemote();

  ExpectCallAdd(
      {{GetURLFromPath(kSubAppIdInvalid), SubAppsServiceResultCode::kFailure}},
      {{kSubAppIdInvalid, kSubAppPath}});
  EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id_).size());
}

// Verify that Add fails for an invalid (non-existing) sub-app.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, AddFailNonExistent) {
  NavigateToParentApp();
  InstallParentApp();
  BindRemote();

  ExpectCallAdd(
      {{GetURLFromPath(kSubAppIdInvalid), SubAppsServiceResultCode::kFailure}},
      {{kSubAppIdInvalid, kSubAppPathInvalid}});
  EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id_).size());
}

// Add call closes the mojo connection if the argument is wrong origin to the
// calling app.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, AddFailWrongOrigin) {
  InstallParentApp();
  NavigateToParentApp();
  BindRemote();

  base::test::TestFuture<void> disconnect_handler_future;
  remote_.set_disconnect_handler(disconnect_handler_future.GetCallback());
  // This call should never succeed and the disconnect handler should be called
  // instead.
  std::vector<SubAppsServiceAddParametersPtr> sub_apps_mojo;
  sub_apps_mojo.emplace_back(
      SubAppsServiceAddParameters::New(kDifferentDomain, kDifferentDomain));
  remote_->Add(std::move(sub_apps_mojo),
               base::BindLambdaForTesting(
                   [](SubAppsServiceImpl::AddResultsMojo results) {
                     ADD_FAILURE() << "Callback unexpectedly invoked.";
                   }));

  ASSERT_TRUE(disconnect_handler_future.Wait())
      << "Disconnect handler not invoked.";
}

// Make sure the Add API can't force manifest update. Add sub-app, verify
// display mode, then add the same one again with different display mode in the
// manifest, and verify that it didn't change.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, AddDoesntForceReinstall) {
  NavigateToParentApp();
  InstallParentApp();
  BindRemote();

  ExpectCallAdd(
      {{GetURLFromPath(kSubAppPath), SubAppsServiceResultCode::kSuccess}},
      {{kSubAppPath, kSubAppPath}});
  AppId sub_app_id = GenerateAppIdFromPath(kSubAppPath);
  EXPECT_EQ(
      DisplayMode::kStandalone,
      provider().registrar_unsafe().GetAppEffectiveDisplayMode(sub_app_id));

  ExpectCallAdd(
      {{GetURLFromPath(kSubAppPath), SubAppsServiceResultCode::kSuccess}},
      {{kSubAppPath, kSubAppPathMinimalUi}});
  EXPECT_EQ(
      DisplayMode::kStandalone,
      provider().registrar_unsafe().GetAppEffectiveDisplayMode(sub_app_id));
}

// Add call should fail if calling app is already a sub app.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, AddFailAppIsSubApp) {
  NavigateToParentApp();
  BindRemote();

  AppId app_id = test::InstallDummyWebApp(
      profile(), "App that is already a sub app",
      GetURLFromPath(kParentAppPath), webapps::WebappInstallSource::SUB_APP);

  ExpectCallAdd(
      {{GetURLFromPath(kSubAppPath), SubAppsServiceResultCode::kFailure}},
      {{kSubAppPath, kSubAppPath}});
  EXPECT_EQ(0ul, GetAllSubAppIds(app_id).size());
}

/******** Tests for the Add API call - adding multiple/zero sub-apps. ********/

// Add one sub-app, verify count is one. Add it again, still same count. Add a
// second sub-app, verify count is two.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, AddTwo) {
  NavigateToParentApp();
  InstallParentApp();

  BindRemote();

  EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id_).size());

  ExpectCallAdd(
      {{GetURLFromPath(kSubAppPath), SubAppsServiceResultCode::kSuccess}},
      {{kSubAppPath, kSubAppPath}});
  EXPECT_EQ(1ul, GetAllSubAppIds(parent_app_id_).size());

  // Try to add first sub app again.
  ExpectCallAdd(
      {{GetURLFromPath(kSubAppPath), SubAppsServiceResultCode::kSuccess}},
      {{kSubAppPath, kSubAppPath}});
  EXPECT_EQ(1ul, GetAllSubAppIds(parent_app_id_).size());

  // Add second sub app.
  ExpectCallAdd(
      {{GetURLFromPath(kSubAppPath2), SubAppsServiceResultCode::kSuccess}},
      {{kSubAppPath2, kSubAppPath2}});
  EXPECT_EQ(2ul, GetAllSubAppIds(parent_app_id_).size());
}

// Verify that Adding multiple sub-apps works correctly.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, AddMultiple) {
  NavigateToParentApp();
  InstallParentApp();
  BindRemote();

  ExpectCallAdd(
      {{GetURLFromPath(kSubAppPath), SubAppsServiceResultCode::kSuccess},
       {GetURLFromPath(kSubAppPath2), SubAppsServiceResultCode::kSuccess},
       {GetURLFromPath(kSubAppPath3), SubAppsServiceResultCode::kSuccess}},
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

  ExpectCallAdd(
      {{GetURLFromPath(kSubAppPath), SubAppsServiceResultCode::kSuccess},
       {GetURLFromPath(kSubAppPathInvalid), SubAppsServiceResultCode::kFailure},
       {GetURLFromPath(kSubAppPath3), SubAppsServiceResultCode::kSuccess}},
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

/******** Tests for the Add API call - dialog behaviour ********/

// Verify that all sub apps are returned with the failure result code when the
// permissions dialog is declined.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest,
                       DialogNotAcceptedReturnsAllSubApps) {
  NavigateToParentApp();
  InstallParentApp();
  BindRemote();

  auto dialog_override =
      SubAppsInstallDialogController::SetAutomaticActionForTesting(
          SubAppsInstallDialogController::DialogActionForTesting::kCancel);

  ExpectCallAdd(
      {{GetURLFromPath(kSubAppPath), SubAppsServiceResultCode::kFailure},
       {GetURLFromPath(kSubAppPath2), SubAppsServiceResultCode::kFailure},
       {GetURLFromPath(kSubAppPath3), SubAppsServiceResultCode::kFailure}},
      {{kSubAppPath, kSubAppPath},
       {kSubAppPath2, kSubAppPath2},
       {kSubAppPath3, kSubAppPath3}});

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

  ExpectCallAdd(
      {{GetURLFromPath(kSubAppPath), SubAppsServiceResultCode::kSuccess},
       {GetURLFromPath(kSubAppPath2), SubAppsServiceResultCode::kSuccess},
       {GetURLFromPath(kSubAppPath3), SubAppsServiceResultCode::kSuccess}},
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
    ScopedRegistryUpdate update = provider().sync_bridge_unsafe().BeginUpdate();
    WebApp* web_app = update->UpdateApp(parent_app_id_);
    ASSERT_TRUE(web_app);
    web_app->AddSource(WebAppManagement::kDefault);
  }

  AppId sub_app_id_1 = GenerateAppIdFromPath(kSubAppPath);
  AppId sub_app_id_2 = GenerateAppIdFromPath(kSubAppPath2);

  ExpectCallAdd(
      {{GetURLFromPath(kSubAppPath), SubAppsServiceResultCode::kSuccess},
       {GetURLFromPath(kSubAppPath2), SubAppsServiceResultCode::kSuccess}},
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
  ExpectCallAdd(
      {{GetURLFromPath(kSubAppPath), SubAppsServiceResultCode::kSuccess}},
      {{kSubAppPath, kSubAppPath}});
  AppId sub_app_id = GenerateAppIdFromPath(kSubAppPath);
  EXPECT_TRUE(provider().registrar_unsafe().IsInstalled(sub_app_id));

  // Add standalone app as sub-app.
  const WebApp* standalone_app =
      provider().registrar_unsafe().GetAppById(standalone_app_id);
  ExpectCallAdd(
      {{GetURLFromPath(kSubAppPath2), SubAppsServiceResultCode::kSuccess}},
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
  EXPECT_EQ(SubAppsServiceResultCode::kSuccess, result->result_code);
  EXPECT_EQ(std::vector<SubAppsServiceListResultEntryPtr>{},
            result->sub_apps_list);

  ExpectCallAdd(
      {{GetURLFromPath(kSubAppPath), SubAppsServiceResultCode::kSuccess},
       {GetURLFromPath(kSubAppPath2), SubAppsServiceResultCode::kSuccess},
       {GetURLFromPath(kSubAppPath3), SubAppsServiceResultCode::kSuccess}},
      {{kSubAppPath, kSubAppPath},
       {kSubAppPath2, kSubAppPath2},
       {kSubAppPath3, kSubAppPath3}});

  result = CallList();

  // We need to use a set for comparison because the ordering changes between
  // invocations (due to embedded test server using a random port each time).
  base::flat_set<SubAppsServiceListResultEntryPtr> expected_set;
  expected_set.emplace(
      SubAppsServiceListResultEntry::New(kSubAppPath, kSubAppName));
  expected_set.emplace(
      SubAppsServiceListResultEntry::New(kSubAppPath2, kSubAppName2));
  expected_set.emplace(
      SubAppsServiceListResultEntry::New(kSubAppPath3, kSubAppName3));

  base::flat_set<SubAppsServiceListResultEntryPtr> actual_set(
      std::make_move_iterator(result->sub_apps_list.begin()),
      std::make_move_iterator(result->sub_apps_list.end()));

  // We see all three sub-apps now.
  EXPECT_EQ(SubAppsServiceResultCode::kSuccess, result->result_code);
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
  ExpectCallAdd(
      {{GetURLFromPath(kSubAppPath2), SubAppsServiceResultCode::kSuccess}},
      {{kSubAppPath2, kSubAppPath2}});

  std::vector<SubAppsServiceListResultEntryPtr> expected_result;
  expected_result.emplace_back(
      SubAppsServiceListResultEntry::New(kSubAppPath2, kSubAppName2));

  // Should only see the sub-app one here, not the standalone.
  SubAppsServiceListResultPtr result = CallList();
  EXPECT_EQ(SubAppsServiceResultCode::kSuccess, result->result_code);
  EXPECT_EQ(expected_result, result->sub_apps_list);
}

// List call returns failure if the parent app isn't installed.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest,
                       ListFailParentAppNotInstalled) {
  NavigateToParentApp();
  BindRemote();

  SubAppsServiceListResultPtr result = CallList();
  EXPECT_EQ(SubAppsServiceResultCode::kFailure, result->result_code);
  EXPECT_EQ(std::vector<SubAppsServiceListResultEntryPtr>{},
            result->sub_apps_list);
}

/********** Tests for the Remove API call. **********/

// Remove works with one app.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, RemoveOneApp) {
  InstallParentApp();
  NavigateToParentApp();
  BindRemote();

  ExpectCallAdd(
      {{GetURLFromPath(kSubAppPath), SubAppsServiceResultCode::kSuccess}},
      {{kSubAppPath, kSubAppPath}});

  AppId app_id = GenerateAppIdFromPath(kSubAppPath);
  EXPECT_EQ(1ul, GetAllSubAppIds(parent_app_id_).size());
  EXPECT_TRUE(provider().registrar_unsafe().IsInstalled(app_id));

  EXPECT_EQ(
      SingleRemoveResultMojo(kSubAppPath, SubAppsServiceResultCode::kSuccess),
      CallRemove({kSubAppPath}));
  EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id_).size());
  EXPECT_FALSE(provider().registrar_unsafe().IsInstalled(app_id));
  EXPECT_TRUE(UninstallNotificationShown());
}

// Remove works with a list of apps.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, RemoveListOfApps) {
  NavigateToParentApp();
  InstallParentApp();
  BindRemote();

  ExpectCallAdd(
      {{GetURLFromPath(kSubAppPath), SubAppsServiceResultCode::kSuccess},
       {GetURLFromPath(kSubAppPath2), SubAppsServiceResultCode::kSuccess}},
      {{kSubAppPath, kSubAppPath}, {kSubAppPath2, kSubAppPath2}});

  EXPECT_EQ(2ul, GetAllSubAppIds(parent_app_id_).size());

  std::vector<std::pair<ManifestId, SubAppsServiceResultCode>> expected_result;
  expected_result.emplace_back(GetURLFromPath(kSubAppPath),
                               SubAppsServiceResultCode::kSuccess);
  expected_result.emplace_back(GetURLFromPath(kSubAppPath2),
                               SubAppsServiceResultCode::kSuccess);
  expected_result.emplace_back(GetURLFromPath(kSubAppPath3),
                               SubAppsServiceResultCode::kFailure);

  EXPECT_THAT(RemoveResultsToList(
                  CallRemove({kSubAppPath, kSubAppPath2, kSubAppPath3})),
              testing::UnorderedElementsAreArray(expected_result));

  EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id_).size());

  ManifestId sub_app_id_1 = GetURLFromPath(kSubAppPath);
  ManifestId sub_app_id_2 = GetURLFromPath(kSubAppPath2);
  EXPECT_FALSE(provider().registrar_unsafe().IsInstalled(
      GenerateAppIdFromManifestId(sub_app_id_1)));
  EXPECT_FALSE(provider().registrar_unsafe().IsInstalled(
      GenerateAppIdFromManifestId(sub_app_id_2)));

  absl::optional<message_center::Notification> uninstall_notification =
      notification_display_service_->GetNotification(
          SubAppsServiceImpl::kSubAppsUninstallNotificationId);
  ASSERT_TRUE(uninstall_notification.has_value());
  // Confirm the string generated for the notification title mentions the
  // correct number of uninstalls (i.e. successful uninstalls rather than
  // the number of requested installs).
  ASSERT_TRUE(uninstall_notification->title().find(u" 2 "));
  ASSERT_TRUE(uninstall_notification->never_timeout());
}

// Calling remove with an empty list doesn't crash.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, RemoveEmptyList) {
  InstallParentApp();
  NavigateToParentApp();
  BindRemote();

  AppId app_id = GenerateAppIdFromPath(kSubAppPath);

  ExpectCallAdd(
      {{GetURLFromPath(kSubAppPath), SubAppsServiceResultCode::kSuccess}},
      {{kSubAppPath, kSubAppPath}});
  EXPECT_EQ(1ul, GetAllSubAppIds(parent_app_id_).size());
  EXPECT_TRUE(provider().registrar_unsafe().IsInstalled(app_id));

  CallRemove({});
  EXPECT_EQ(1ul, GetAllSubAppIds(parent_app_id_).size());
  EXPECT_TRUE(provider().registrar_unsafe().IsInstalled(app_id));
  EXPECT_FALSE(UninstallNotificationShown());
}

// Remove fails for a regular installed app.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, RemoveFailRegularApp) {
  // Regular install.
  InstallPWAFromPath(kSubAppPath);

  InstallParentApp();
  NavigateToParentApp();
  BindRemote();

  EXPECT_EQ(
      SingleRemoveResultMojo(kSubAppPath, SubAppsServiceResultCode::kFailure),
      CallRemove({kSubAppPath}));
  EXPECT_FALSE(UninstallNotificationShown());
}

// Remove fails for a sub-app with a different parent_app_id.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, RemoveFailWrongParent) {
  // SubApp plays the parent app here, SubApp2 is its sub-app, SubApp3 is the
  // other "parent app".
  AppId parent_app = InstallPWAFromPath(kSubAppPath);
  NavigateToPath(kSubAppPath);
  BindRemote();

  ExpectCallAdd(
      {{GetURLFromPath(kSubAppPath2), SubAppsServiceResultCode::kSuccess}},
      {{kSubAppPath2, kSubAppPath2}});

  AppId second_parent_app = InstallPWAFromPath(kSubAppPath3);
  NavigateToPath(kSubAppPath3);
  remote_.reset();
  BindRemote();

  EXPECT_EQ(
      SingleRemoveResultMojo(kSubAppPath2, SubAppsServiceResultCode::kFailure),
      CallRemove({kSubAppPath2}));
  EXPECT_FALSE(UninstallNotificationShown());
}

// Remove call returns failure if the calling app isn't installed.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest,
                       RemoveFailCallingAppNotInstalled) {
  NavigateToParentApp();
  BindRemote();

  EXPECT_EQ(
      SingleRemoveResultMojo(kSubAppPath, SubAppsServiceResultCode::kFailure),
      CallRemove({kSubAppPath}));
  EXPECT_FALSE(UninstallNotificationShown());
}

// Remove call closes the mojo connection if the argument is wrong origin to the
// calling app.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, RemoveFailWrongOrigin) {
  InstallParentApp();
  NavigateToParentApp();
  BindRemote();

  base::test::TestFuture<void> disconnect_handler_future;
  remote_.set_disconnect_handler(disconnect_handler_future.GetCallback());
  // This call should never succeed and the disconnect handler should be called
  // instead.
  remote_->Remove({kDifferentDomain},
                  base::BindLambdaForTesting(
                      [](std::vector<SubAppsServiceRemoveResultPtr> result) {
                        ADD_FAILURE() << "Callback unexpectedly invoked.";
                      }));
  ASSERT_TRUE(disconnect_handler_future.Wait())
      << "Disconnect handler not invoked.";
  EXPECT_FALSE(UninstallNotificationShown());
}

}  // namespace web_app
