// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/sub_apps_service_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_future.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/sub_apps_install_dialog_controller.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
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
#include "components/permissions/permission_decision_auto_blocker.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"
#include "url/origin.h"

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
constexpr const char kSubAppPath[] = "/sub1/page.html";
constexpr const char kSubAppName[] = "sub1";
constexpr const char kSubAppPathMinimalUi[] =
    "/web_apps/standalone/basic.html?manifest=manifest_minimal_ui.json";
constexpr const char kSubAppPath2[] = "/sub2/page.html";
constexpr const char kSubAppName2[] = "sub2";
constexpr const char kSubAppPath3[] = "/sub3/page.html";
constexpr const char kSubAppName3[] = "sub3";
constexpr const char kSubAppPathInvalid[] = "/invalid/sub/app/path.html";
constexpr const char kSubAppIdInvalid[] = "/invalid-sub-app-id";

constexpr const char kSub1[] = "/sub1/page.html";
constexpr const char kSub2[] = "/sub2/page.html";

}  // namespace

using RemoveResultsMojo =
    std::vector<blink::mojom::SubAppsServiceRemoveResultPtr>;

using AddResults = std::vector<
    std::pair<webapps::ManifestId, blink::mojom::SubAppsServiceResultCode>>;

// There's one simple end-to-end test that actually calls the JS API interface,
// the rest test the mojo interface (since the first layer listening to the API
// calls is almost a direct passthrough to the mojo service).
//
// JS API interface tests are in
// third_party/blink/web_tests/external/wpt/subapps/.

class SubAppsServiceImplBrowserTest : public IsolatedWebAppBrowserTestHarness {
 public:
  SubAppsServiceImplBrowserTest()
      : dialog_override_(
            SubAppsInstallDialogController::SetAutomaticActionForTesting(
                SubAppsInstallDialogController::DialogActionForTesting::
                    kAccept)) {}
  void SetUpOnMainThread() override {
    IsolatedWebAppBrowserTestHarness::SetUpOnMainThread();
    notification_display_service_ =
        std::make_unique<NotificationDisplayServiceTester>(profile());
  }

  void TearDownOnMainThread() override {
    notification_display_service_.reset();
    IsolatedWebAppBrowserTestHarness::TearDownOnMainThread();
  }

  content::RenderFrameHost* render_frame_host(
      content::WebContents* web_contents = nullptr) {
    if (!web_contents) {
      web_contents = browser()->tab_strip_model()->GetActiveWebContents();
    }
    return web_contents->GetPrimaryMainFrame();
  }

  webapps::ManifestId parent_manifest_id() {
    return provider().registrar_unsafe().GetAppManifestId(parent_app_id_);
  }

  GURL GetURLFromPath(const std::string& path) {
    return https_server()->GetURL(kDomain, path);
  }

  GURL GetURLFromPath(const std::string& path,
                      const content::RenderFrameHost* parent_frame) {
    return GURL(parent_frame->GetLastCommittedOrigin().GetURL().Resolve(path));
  }

  webapps::AppId GenerateSubAppIdFromPath(
      const std::string& path,
      const content::RenderFrameHost* parent_frame,
      const webapps::ManifestId& parent_manifest_id) {
    return GenerateAppId(/*manifest_id_path=*/std::nullopt,
                         GetURLFromPath(path, parent_frame),
                         parent_manifest_id);
  }

  webapps::AppId InstallPwaFromPath(const std::string& path) {
    return InstallPWA(GetURLFromPath(path));
  }

  content::RenderFrameHost* InstallAndOpenParentIwaApp() {
    IsolatedWebAppUrlInfo parent_app = InstallIwaParentApp();
    return OpenApp(parent_app.app_id());
  }

  IsolatedWebAppUrlInfo InstallIwaParentApp() {
    iwa_dev_server_ = CreateAndStartDevServer(
        FILE_PATH_LITERAL("web_apps/subapps_isolated_app"));
    IsolatedWebAppUrlInfo parent_app =
        web_app::InstallDevModeProxyIsolatedWebApp(
            browser()->profile(), iwa_dev_server_->GetOrigin());
    parent_app_id_ = parent_app.app_id();

    EXPECT_TRUE(provider().registrar_unsafe().IsInstalled(parent_app_id_));
    EXPECT_TRUE(provider().registrar_unsafe().IsIsolated(parent_app_id_));
    EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id_).size());

    return parent_app;
  }

  // sub_app_paths should contain paths, not full URLs.
  bool AddSubAppsJS(content::RenderFrameHost* frame,
                    const std::vector<std::string>& sub_app_paths) const {
    std::string script = "navigator.subApps.add({";
    for (std::string path : sub_app_paths) {
      base::StringAppendF(&script, R"("%s": {"installURL": "%s"},)", &path[0],
                          &path[0]);
    }
    script += " })";

    return content::ExecJs(frame, script);
  }

  content::EvalJsResult ListSubAppsJS(content::RenderFrameHost* frame) const {
    return content::EvalJs(frame, "navigator.subApps.list()");
  }

  void UninstallParentApp() { UninstallWebApp(parent_app_id_); }

  void UninstallParentAppBySource(WebAppManagement::Type source) {
    base::test::TestFuture<void> uninstall_future;
    provider().scheduler().RemoveInstallManagementMaybeUninstall(
        parent_app_id_, source, webapps::WebappUninstallSource::kAppsPage,
        base::BindLambdaForTesting([&](webapps::UninstallResultCode code) {
          EXPECT_EQ(code, webapps::UninstallResultCode::kInstallSourceRemoved);
          uninstall_future.SetValue();
        }));
    ASSERT_TRUE(uninstall_future.Wait())
        << "UninstallExternalWebApp did not trigger the callback.";
  }

  std::vector<webapps::AppId> GetAllSubAppIds(
      const webapps::AppId& parent_app_id) {
    return provider().registrar_unsafe().GetAllSubAppIds(parent_app_id);
  }

  void BindRemote(content::RenderFrameHost* frame) {
    // Any navigation causes the remote to be destroyed (since the
    // render_frame_host that owns it gets destroyed.)
    SubAppsServiceImpl::CreateIfAllowed(frame,
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
      base::flat_set<std::pair<webapps::ManifestId, SubAppsServiceResultCode>>
          expected,
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

  std::vector<std::pair<webapps::ManifestId, SubAppsServiceResultCode>>
  RemoveResultsToList(RemoveResultsMojo results) {
    std::vector<std::pair<webapps::ManifestId, SubAppsServiceResultCode>> list;
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

  base::SimpleTestClock* clock() { return &clock_; }

 protected:
  base::test::ScopedFeatureList features_{blink::features::kDesktopPWAsSubApps};
  webapps::AppId parent_app_id_;
  webapps::ManifestId parent_app_manifest_id_;
  mojo::Remote<SubAppsService> remote_;
  base::AutoReset<
      std::optional<SubAppsInstallDialogController::DialogActionForTesting>>
      dialog_override_;
  std::unique_ptr<net::EmbeddedTestServer> iwa_dev_server_;
  std::unique_ptr<NotificationDisplayServiceTester>
      notification_display_service_;
  base::SimpleTestClock clock_;
};

/********** End-to-end test (one is enough!). **********/

// Simple end-to-end test for add().
// NOTE: Only one E2E test is enough, test everything else through the Mojo
// interface (as all the other tests do).
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, EndToEndAdd) {
  content::RenderFrameHost* iwa_frame = InstallAndOpenParentIwaApp();

  EXPECT_TRUE(AddSubAppsJS(iwa_frame, {kSub1, kSub2}));
  EXPECT_EQ(2ul, GetAllSubAppIds(parent_app_id_).size());
}

IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest,
                       ApiUndefinedForNonIsolatedApp) {
  auto* frame =
      ui_test_utils::NavigateToURL(browser(), GetURLFromPath(kParentAppPath));
  ASSERT_TRUE(frame);

  auto result = content::ExecJs(frame, "navigator.subApps.add({})");
  EXPECT_TRUE(result.failure_message());
  std::string message = result.failure_message();
  EXPECT_NE(message.find("Cannot read properties of undefined (reading 'add')"),
            std::string::npos);
}

/********** Tests for the Add API call. **********/

// Add a single sub-app and verify all sorts of things.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, AddSingle) {
  // Dependency graph:
  // NavigateToParentApp --> BindRemote --> CallAdd
  //                   \---------------->/
  // InstallParentApp ----------------->/
  content::RenderFrameHost* iwa_frame = InstallAndOpenParentIwaApp();

  BindRemote(iwa_frame);
  EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id_).size());

  ExpectCallAdd({{GetURLFromPath(kSub1), SubAppsServiceResultCode::kSuccess}},
                {{kSub1, kSub1}});

  // Verify a bunch of things for the newly installed sub-app.
  webapps::AppId sub_app_id =
      GenerateSubAppIdFromPath(kSub1, iwa_frame, parent_manifest_id());

  EXPECT_TRUE(provider().registrar_unsafe().IsInstalled(sub_app_id));
  EXPECT_TRUE(provider().registrar_unsafe().IsInstallState(
      sub_app_id, {proto::INSTALLED_WITHOUT_OS_INTEGRATION,
                   proto::INSTALLED_WITH_OS_INTEGRATION}));
  EXPECT_EQ(
      DisplayMode::kStandalone,
      provider().registrar_unsafe().GetAppEffectiveDisplayMode(sub_app_id));

  const WebApp* sub_app = provider().registrar_unsafe().GetAppById(sub_app_id);
  EXPECT_EQ(sub_app_id, sub_app->app_id());
  EXPECT_EQ(parent_app_id_, sub_app->parent_app_id());
  EXPECT_EQ(std::vector<webapps::AppId>{sub_app->app_id()},
            GetAllSubAppIds(parent_app_id_));
  EXPECT_TRUE(sub_app->IsSubAppInstalledApp());
  EXPECT_TRUE(sub_app->CanUserUninstallWebApp());
  EXPECT_EQ(GetURLFromPath(kSub1, iwa_frame), sub_app->start_url());
  if (provider().ui_manager().CanAddAppToQuickLaunchBar()) {
    EXPECT_FALSE(provider().ui_manager().IsAppInQuickLaunchBar(sub_app_id));
  }
}

// Verify that Add works if PWA is launched as standalone window.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, AddStandaloneWindow) {
  content::RenderFrameHost* iwa_frame = InstallAndOpenParentIwaApp();
  BindRemote(iwa_frame);

  ExpectCallAdd(
      {{GetURLFromPath(kSubAppPath), SubAppsServiceResultCode::kSuccess}},
      {{kSubAppPath, kSubAppPath}});
}

// Verify that adding the same app as standalone and as subapp results in two
// separate apps being registered with different manifest_id and app_id.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest,
                       AddSameAppAsSubAndStandalone) {
  content::RenderFrameHost* iwa_frame = InstallAndOpenParentIwaApp();
  BindRemote(iwa_frame);

  ExpectCallAdd(
      {{GetURLFromPath(kSubAppPath), SubAppsServiceResultCode::kSuccess}},
      {{kSubAppPath, kSubAppPath}});

  webapps::AppId standalone_app_id = InstallPwaFromPath(kSubAppPath);
  webapps::AppId sub_app_id =
      GenerateSubAppIdFromPath(kSubAppPath, iwa_frame, parent_manifest_id());

  EXPECT_TRUE(provider().registrar_unsafe().IsInstalled(sub_app_id));
  EXPECT_TRUE(provider().registrar_unsafe().IsInstalled(standalone_app_id));

  EXPECT_EQ(1ul, GetAllSubAppIds(parent_app_id_).size());
  EXPECT_EQ(sub_app_id, GetAllSubAppIds(parent_app_id_)[0]);

  const WebApp* sub_app = provider().registrar_unsafe().GetAppById(sub_app_id);
  const WebApp* standalone_app =
      provider().registrar_unsafe().GetAppById(standalone_app_id);

  EXPECT_TRUE(sub_app->HasOnlySource(WebAppManagement::kSubApp));

  // Manifest ids and app ids are different.
  EXPECT_NE(sub_app->manifest_id(), standalone_app->manifest_id());
  EXPECT_NE(sub_app_id, standalone_app_id);

  CallRemove({kSubAppPath});
  EXPECT_FALSE(provider().registrar_unsafe().IsInstalled(sub_app_id));
  EXPECT_TRUE(provider().registrar_unsafe().IsInstalled(standalone_app_id));

  // Inverting the order of installations.
  webapps::AppId standalone_app_id2 = InstallPwaFromPath(kSubAppPath2);
  webapps::AppId sub_app_id2 =
      GenerateSubAppIdFromPath(kSubAppPath2, iwa_frame, parent_manifest_id());
  ExpectCallAdd(
      {{GetURLFromPath(kSubAppPath2), SubAppsServiceResultCode::kSuccess}},
      {{kSubAppPath2, kSubAppPath2}});

  EXPECT_TRUE(provider().registrar_unsafe().IsInstalled(sub_app_id2));
  EXPECT_TRUE(provider().registrar_unsafe().IsInstalled(standalone_app_id2));

  EXPECT_EQ(1ul, GetAllSubAppIds(parent_app_id_).size());
  EXPECT_EQ(sub_app_id2, GetAllSubAppIds(parent_app_id_)[0]);

  const WebApp* sub_app2 =
      provider().registrar_unsafe().GetAppById(sub_app_id2);
  const WebApp* standalone_app2 =
      provider().registrar_unsafe().GetAppById(standalone_app_id2);

  EXPECT_TRUE(sub_app2->HasOnlySource(WebAppManagement::kSubApp));

  // Manifest ids and app ids are different.
  EXPECT_NE(sub_app2->manifest_id(), standalone_app2->manifest_id());
  EXPECT_NE(sub_app_id2, standalone_app_id2);

  CallRemove({kSubAppPath2});
  EXPECT_FALSE(provider().registrar_unsafe().IsInstalled(sub_app_id2));
  EXPECT_TRUE(provider().registrar_unsafe().IsInstalled(standalone_app_id2));
}

// Add call should fail if the parent app isn't installed.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest,
                       AddFailParentAppNotInstalled) {
  content::RenderFrameHost* iwa_frame = InstallAndOpenParentIwaApp();
  BindRemote(iwa_frame);

  {
    auto* sync_bridge = &provider().sync_bridge_unsafe();
    auto update = sync_bridge->BeginUpdate();
    update->DeleteApp(parent_app_id_);
  }

  ExpectCallAdd(
      {{GetURLFromPath(kSubAppPath), SubAppsServiceResultCode::kFailure}},
      {{kSubAppPath, kSubAppPath}});
}

// Add call should fail if the call wasn't made from the context of parent app.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest,
                       AddFailNotInParentAppContext) {
  IsolatedWebAppUrlInfo parent_app = InstallIwaParentApp();

  auto new_contents = content::WebContents::Create(
      content::WebContents::CreateParams(profile()));
  auto* frame = new_contents->GetPrimaryMainFrame();
  remote_.reset();
  BindRemote(frame);
  browser()->tab_strip_model()->AppendWebContents(std::move(new_contents),
                                                  /*foreground=*/true);

  base::test::TestFuture<void> disconnect_handler_future;
  remote_.set_disconnect_handler(disconnect_handler_future.GetCallback());

  std::vector<SubAppsServiceAddParametersPtr> sub_apps_mojo;
  sub_apps_mojo.emplace_back(
      SubAppsServiceAddParameters::New(kSubAppPath, kSubAppPath));
  remote_->Add(std::move(sub_apps_mojo),
               base::BindLambdaForTesting(
                   [](SubAppsServiceImpl::AddResultsMojo results) {
                     ADD_FAILURE() << "Callback unexpectedly invoked.";
                   }));

  ASSERT_TRUE(disconnect_handler_future.Wait())
      << "Disconnect handler not invoked.";
}

// Verify that Add call rejects a sub-app with the wrong specified app_id.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, AddFailIncorrectId) {
  content::RenderFrameHost* iwa_frame = InstallAndOpenParentIwaApp();
  BindRemote(iwa_frame);

  ExpectCallAdd(
      {{GetURLFromPath(kSubAppIdInvalid), SubAppsServiceResultCode::kFailure}},
      {{kSubAppIdInvalid, kSubAppPath}});
  EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id_).size());
}

// Verify that Add fails when trying to add the parent app as sub-app of
// itself.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, AddFailForParentApp) {
  content::RenderFrameHost* iwa_frame = InstallAndOpenParentIwaApp();
  BindRemote(iwa_frame);

  ExpectCallAdd({{GetURLFromPath("/"), SubAppsServiceResultCode::kFailure}},
                {{"", ""}});
  EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id_).size());
}

// Verify that Add fails for an invalid (non-existing) sub-app.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, AddFailNonExistent) {
  content::RenderFrameHost* iwa_frame = InstallAndOpenParentIwaApp();
  BindRemote(iwa_frame);

  ExpectCallAdd(
      {{GetURLFromPath(kSubAppIdInvalid), SubAppsServiceResultCode::kFailure}},
      {{kSubAppIdInvalid, kSubAppPathInvalid}});
  EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id_).size());
}

// Add call closes the mojo connection if the argument is wrong origin to the
// calling app.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, AddFailWrongOrigin) {
  content::RenderFrameHost* iwa_frame = InstallAndOpenParentIwaApp();
  BindRemote(iwa_frame);

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
  content::RenderFrameHost* iwa_frame = InstallAndOpenParentIwaApp();
  BindRemote(iwa_frame);

  ExpectCallAdd(
      {{GetURLFromPath(kSubAppPath), SubAppsServiceResultCode::kSuccess}},
      {{kSubAppPath, kSubAppPath}});
  webapps::AppId sub_app_id =
      GenerateSubAppIdFromPath(kSubAppPath, iwa_frame, parent_manifest_id());
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
  content::RenderFrameHost* iwa_frame = InstallAndOpenParentIwaApp();
  auto* original_provider = WebAppProvider::GetForTest(profile());
  BindRemote(iwa_frame);

  ExpectCallAdd(
      {{GetURLFromPath(kSubAppPath), SubAppsServiceResultCode::kSuccess}},
      {{kSubAppPath, kSubAppPath}});
  EXPECT_EQ(1ul, GetAllSubAppIds(parent_app_id_).size());

  CloseAllBrowsers();

  auto sub_app_id = GetAllSubAppIds(parent_app_id_)[0];
  EXPECT_EQ(0ul, GetAllSubAppIds(sub_app_id).size());
  content::RenderFrameHost* sub_app_iwa_frame = OpenApp(sub_app_id);
  remote_.reset();
  BindRemote(sub_app_iwa_frame);

  base::test::TestFuture<void> disconnect_handler_future;
  remote_.set_disconnect_handler(disconnect_handler_future.GetCallback());

  std::vector<SubAppsServiceAddParametersPtr> sub_apps_mojo;
  sub_apps_mojo.emplace_back(
      SubAppsServiceAddParameters::New(kSubAppPath2, kSubAppPath2));
  remote_->Add(std::move(sub_apps_mojo),
               base::BindLambdaForTesting(
                   [](SubAppsServiceImpl::AddResultsMojo results) {
                     ADD_FAILURE() << "Callback unexpectedly invoked.";
                   }));

  ASSERT_TRUE(disconnect_handler_future.Wait())
      << "Disconnect handler not invoked.";

  // After the browser crashes, the profile is invalid and the call to get the
  // provider fails. We can reuse the provider created at the beginning of the
  // test to verify the status of installed subapps.
  EXPECT_EQ(1ul, original_provider->registrar_unsafe()
                     .GetAllSubAppIds(parent_app_id_)
                     .size());
  EXPECT_EQ(
      0ul,
      original_provider->registrar_unsafe().GetAllSubAppIds(sub_app_id).size());
}

/******** Tests for the Add API call - adding multiple/zero sub-apps. ********/

// Add one sub-app, verify count is one. Add it again, still same count. Add a
// second sub-app, verify count is two.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, AddTwo) {
  content::RenderFrameHost* iwa_frame = InstallAndOpenParentIwaApp();
  BindRemote(iwa_frame);

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
  content::RenderFrameHost* iwa_frame = InstallAndOpenParentIwaApp();
  BindRemote(iwa_frame);

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
  content::RenderFrameHost* iwa_frame = InstallAndOpenParentIwaApp();
  BindRemote(iwa_frame);

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
  content::RenderFrameHost* iwa_frame = InstallAndOpenParentIwaApp();
  BindRemote(iwa_frame);

  ExpectCallAdd({}, {});
  EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id_).size());
}

/******** Tests for the Add API call - dialog behaviour ********/

// Verify that all sub apps are returned with the failure result code when the
// permissions dialog is declined.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest,
                       DialogNotAcceptedReturnsAllSubApps) {
  content::RenderFrameHost* iwa_frame = InstallAndOpenParentIwaApp();
  BindRemote(iwa_frame);

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

IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest,
                       DialogEmbargoedIfDeclinedThreeTimes) {
  content::RenderFrameHost* iwa_frame = InstallAndOpenParentIwaApp();
  BindRemote(iwa_frame);

  auto dialog_override =
      SubAppsInstallDialogController::SetAutomaticActionForTesting(
          SubAppsInstallDialogController::DialogActionForTesting::kCancel);

  std::vector<std::pair<std::string, std::string>> subapps = {
      {kSubAppPath, kSubAppPath},
      {kSubAppPath2, kSubAppPath2},
      {kSubAppPath3, kSubAppPath3},
  };
  base::flat_set<std::pair<webapps::ManifestId, SubAppsServiceResultCode>>
      expected_result = {
          {GetURLFromPath(kSubAppPath), SubAppsServiceResultCode::kFailure},
          {GetURLFromPath(kSubAppPath2), SubAppsServiceResultCode::kFailure},
          {GetURLFromPath(kSubAppPath3), SubAppsServiceResultCode::kFailure}};

  // Dismiss dialog three times.
  for (int i = 0; i < 3; i++) {
    ExpectCallAdd(expected_result, subapps);
    EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id_).size());
  }

  EXPECT_TRUE(
      PermissionDecisionAutoBlockerFactory::GetForProfile(profile())
          ->IsEmbargoed(iwa_frame->GetLastCommittedOrigin().GetURL(),
                        ContentSettingsType::SUB_APP_INSTALLATION_PROMPTS));

  dialog_override =
      SubAppsInstallDialogController::SetAutomaticActionForTesting(
          SubAppsInstallDialogController::DialogActionForTesting::kAccept);

  // Add call fails now even though we would accept because the dialog was
  // embargoed.
  ExpectCallAdd(expected_result, subapps);

  EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id_).size());
}

IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, DialogEmbargoTiming) {
  content::RenderFrameHost* iwa_frame = InstallAndOpenParentIwaApp();
  BindRemote(iwa_frame);

  // Always hit "Cancel" in the dialog.
  auto dialog_override =
      SubAppsInstallDialogController::SetAutomaticActionForTesting(
          SubAppsInstallDialogController::DialogActionForTesting::kCancel);

  auto* auto_blocker =
      PermissionDecisionAutoBlockerFactory::GetForProfile(profile());
  auto_blocker->SetClockForTesting(clock());

  std::vector<std::pair<std::string, std::string>> subapps = {
      {kSubAppPath, kSubAppPath},
      {kSubAppPath2, kSubAppPath2},
      {kSubAppPath3, kSubAppPath3}};
  base::flat_set<std::pair<webapps::ManifestId, SubAppsServiceResultCode>>
      expected_result = {
          {GetURLFromPath(kSubAppPath), SubAppsServiceResultCode::kFailure},
          {GetURLFromPath(kSubAppPath2), SubAppsServiceResultCode::kFailure},
          {GetURLFromPath(kSubAppPath3), SubAppsServiceResultCode::kFailure}};

  // Dismiss dialog three times.
  for (int i = 0; i < 3; i++) {
    ExpectCallAdd(expected_result, subapps);
    EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id_).size());
  }

  // Check that embargo lasts for 10 minutes.
  EXPECT_TRUE(auto_blocker->IsEmbargoed(
      iwa_frame->GetLastCommittedOrigin().GetURL(),
      ContentSettingsType::SUB_APP_INSTALLATION_PROMPTS));

  clock()->Advance(base::Minutes(9));
  EXPECT_TRUE(auto_blocker->IsEmbargoed(
      iwa_frame->GetLastCommittedOrigin().GetURL(),
      ContentSettingsType::SUB_APP_INSTALLATION_PROMPTS));

  clock()->Advance(base::Minutes(1));
  EXPECT_FALSE(auto_blocker->IsEmbargoed(
      iwa_frame->GetLastCommittedOrigin().GetURL(),
      ContentSettingsType::SUB_APP_INSTALLATION_PROMPTS));

  // Dismiss forth time.
  ExpectCallAdd(expected_result, subapps);
  EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id_).size());

  // Check that embargo now lasts for 7 days.
  EXPECT_TRUE(auto_blocker->IsEmbargoed(
      iwa_frame->GetLastCommittedOrigin().GetURL(),
      ContentSettingsType::SUB_APP_INSTALLATION_PROMPTS));

  clock()->Advance(base::Days(6));
  EXPECT_TRUE(auto_blocker->IsEmbargoed(
      iwa_frame->GetLastCommittedOrigin().GetURL(),
      ContentSettingsType::SUB_APP_INSTALLATION_PROMPTS));

  clock()->Advance(base::Days(7));
  EXPECT_FALSE(auto_blocker->IsEmbargoed(
      iwa_frame->GetLastCommittedOrigin().GetURL(),
      ContentSettingsType::SUB_APP_INSTALLATION_PROMPTS));
}
/********** Tests for uninstallation behaviour. **********/

// Verify that uninstalling an app with sub-apps causes sub-apps to be
// uninstalled as well.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest,
                       UninstallingParentAppUninstallsSubApps) {
  content::RenderFrameHost* iwa_frame = InstallAndOpenParentIwaApp();
  BindRemote(iwa_frame);

  ExpectCallAdd(
      {{GetURLFromPath(kSubAppPath), SubAppsServiceResultCode::kSuccess},
       {GetURLFromPath(kSubAppPath2), SubAppsServiceResultCode::kSuccess},
       {GetURLFromPath(kSubAppPath3), SubAppsServiceResultCode::kSuccess}},
      {{kSubAppPath, kSubAppPath},
       {kSubAppPath2, kSubAppPath2},
       {kSubAppPath3, kSubAppPath3}});

  // Verify that sub-apps are installed.
  webapps::AppId sub_app_id_1 =
      GenerateSubAppIdFromPath(kSubAppPath, iwa_frame, parent_manifest_id());
  webapps::AppId sub_app_id_2 =
      GenerateSubAppIdFromPath(kSubAppPath2, iwa_frame, parent_manifest_id());
  webapps::AppId sub_app_id_3 =
      GenerateSubAppIdFromPath(kSubAppPath3, iwa_frame, parent_manifest_id());

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
  content::RenderFrameHost* iwa_frame = InstallAndOpenParentIwaApp();
  BindRemote(iwa_frame);

  // Add another source to the parent app.
  {
    ScopedRegistryUpdate update = provider().sync_bridge_unsafe().BeginUpdate();
    WebApp* web_app = update->UpdateApp(parent_app_id_);
    ASSERT_TRUE(web_app);
    web_app->AddSource(WebAppManagement::kDefault);
  }

  ExpectCallAdd(
      {{GetURLFromPath(kSubAppPath), SubAppsServiceResultCode::kSuccess},
       {GetURLFromPath(kSubAppPath2), SubAppsServiceResultCode::kSuccess}},
      {{kSubAppPath, kSubAppPath}, {kSubAppPath2, kSubAppPath2}});

  // Verify that 2 sub-apps are installed.
  webapps::AppId sub_app_id_1 =
      GenerateSubAppIdFromPath(kSubAppPath, iwa_frame, parent_manifest_id());
  webapps::AppId sub_app_id_2 =
      GenerateSubAppIdFromPath(kSubAppPath2, iwa_frame, parent_manifest_id());
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
// source removes all the "sub-app" install sources and uninstalls it.
IN_PROC_BROWSER_TEST_F(
    SubAppsServiceImplBrowserTest,
    UninstallingParentAppUninstallsOnlySubAppIfMultipleSources) {
  content::RenderFrameHost* iwa_frame = InstallAndOpenParentIwaApp();
  BindRemote(iwa_frame);

  // Install app as standalone app.
  webapps::AppId standalone_app_id = InstallPwaFromPath(kSubAppPath2);
  EXPECT_TRUE(provider().registrar_unsafe().IsInstalled(standalone_app_id));

  // Add another sub-app to verify standalone app install/uninstall does not
  // affect normal sub-app uninstalls.
  ExpectCallAdd(
      {{GetURLFromPath(kSubAppPath), SubAppsServiceResultCode::kSuccess}},
      {{kSubAppPath, kSubAppPath}});

  webapps::AppId sub_app_id =
      GenerateSubAppIdFromPath(kSubAppPath, iwa_frame, parent_manifest_id());
  EXPECT_TRUE(provider().registrar_unsafe().IsInstalled(sub_app_id));

  // Add standalone app as sub-app.
  ExpectCallAdd(
      {{GetURLFromPath(kSubAppPath2), SubAppsServiceResultCode::kSuccess}},
      {{kSubAppPath2, kSubAppPath2}});

  EXPECT_EQ(2ul, GetAllSubAppIds(parent_app_id_).size());

  const WebApp* standalone_app =
      provider().registrar_unsafe().GetAppById(standalone_app_id);

  // Verify that the stadalone install is NOT installed and registered as a
  // sub-app.
  EXPECT_NE(parent_app_id_, standalone_app->parent_app_id());
  EXPECT_TRUE(WebAppManagementTypes(
                  {WebAppManagement::kSync, WebAppManagement::kUserInstalled})
                  .HasAll(standalone_app->GetSources()));
  EXPECT_FALSE(standalone_app->IsSubAppInstalledApp());

  UninstallParentApp();

  // Verify that the second sub-app is uninstalled.
  EXPECT_FALSE(provider().registrar_unsafe().IsInstalled(sub_app_id));

  // Verify that previous standalone is still installed.
  EXPECT_TRUE(provider().registrar_unsafe().IsInstalled(standalone_app_id));

  // Verify that there are no apps registered as parent app's sub apps.
  EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id_).size());
  EXPECT_EQ(std::nullopt, standalone_app->parent_app_id());

  // Verify that the standalone app no longer has the sub-app install source.
  EXPECT_TRUE(WebAppManagementTypes(
                  {WebAppManagement::kSync, WebAppManagement::kUserInstalled})
                  .HasAll(standalone_app->GetSources()));
}

/********** Tests for the List API call. **********/

// List call returns the correct value for three sub-apps.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, ListSuccess) {
  content::RenderFrameHost* iwa_frame = InstallAndOpenParentIwaApp();
  BindRemote(iwa_frame);

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
  InstallPwaFromPath(kSubAppPath);

  content::RenderFrameHost* iwa_frame = InstallAndOpenParentIwaApp();
  BindRemote(iwa_frame);

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
  EXPECT_EQ(1ul, result->sub_apps_list.size());
  EXPECT_EQ(expected_result, result->sub_apps_list);
}

// List call returns failure if the parent app isn't installed.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest,
                       ListFailParentAppNotInstalled) {
  content::RenderFrameHost* iwa_frame = InstallAndOpenParentIwaApp();
  BindRemote(iwa_frame);

  {
    auto* sync_bridge = &provider().sync_bridge_unsafe();
    auto update = sync_bridge->BeginUpdate();
    update->DeleteApp(parent_app_id_);
  }

  SubAppsServiceListResultPtr result = CallList();
  EXPECT_EQ(SubAppsServiceResultCode::kFailure, result->result_code);
  EXPECT_EQ(std::vector<SubAppsServiceListResultEntryPtr>{},
            result->sub_apps_list);
}

// Verify that List does not return subapps installed outside of the parent app
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest,
                       ListReturnsOnlyAppsInstalledByTheCurrentParent) {
  content::RenderFrameHost* iwa_frame_1 = InstallAndOpenParentIwaApp();

  // Install the parent app a second time. InstallDevModeProxyIsolatedWebApp
  // generates a new random app id, making the 2 installs effectively 2
  // different apps.
  IsolatedWebAppUrlInfo parent_app_2 =
      web_app::InstallDevModeProxyIsolatedWebApp(browser()->profile(),
                                                 iwa_dev_server_->GetOrigin());
  content::RenderFrameHost* iwa_frame_2 = OpenApp(parent_app_2.app_id());

  EXPECT_TRUE(provider().registrar_unsafe().IsInstalled(parent_app_id_));
  EXPECT_TRUE(provider().registrar_unsafe().IsInstalled(parent_app_2.app_id()));
  EXPECT_NE(parent_app_id_, parent_app_2.app_id());

  // Call Add for both IWAs.
  EXPECT_TRUE(AddSubAppsJS(iwa_frame_1, {kSubAppPath, kSubAppPath2}));
  EXPECT_TRUE(AddSubAppsJS(iwa_frame_2, {}));
  EXPECT_TRUE(AddSubAppsJS(iwa_frame_1, {kSubAppPath, kSubAppPath3}));

  // Check List results for the main app contains 3 sub-apps.
  auto list_result_1 = ListSubAppsJS(iwa_frame_1);
  EXPECT_TRUE(list_result_1.error.empty());

  auto& dict_1 = list_result_1.value.GetDict();
  EXPECT_EQ(3ul, dict_1.size());
  EXPECT_TRUE(dict_1.contains(kSubAppPath));
  EXPECT_TRUE(dict_1.contains(kSubAppPath2));
  EXPECT_TRUE(dict_1.contains(kSubAppPath3));

  // Check List results for the second app is empty.
  auto list_result_2 = ListSubAppsJS(iwa_frame_2);
  EXPECT_TRUE(list_result_2.error.empty());
  EXPECT_EQ(0ul, list_result_2.value.GetDict().size());
}

/********** Tests for the Remove API call. **********/

// Remove works with one app.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, RemoveOneApp) {
  content::RenderFrameHost* iwa_frame = InstallAndOpenParentIwaApp();
  BindRemote(iwa_frame);

  ExpectCallAdd(
      {{GetURLFromPath(kSubAppPath), SubAppsServiceResultCode::kSuccess}},
      {{kSubAppPath, kSubAppPath}});

  webapps::AppId app_id =
      GenerateSubAppIdFromPath(kSubAppPath, iwa_frame, parent_manifest_id());
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
  content::RenderFrameHost* iwa_frame = InstallAndOpenParentIwaApp();
  BindRemote(iwa_frame);

  ExpectCallAdd(
      {{GetURLFromPath(kSubAppPath), SubAppsServiceResultCode::kSuccess},
       {GetURLFromPath(kSubAppPath2), SubAppsServiceResultCode::kSuccess}},
      {{kSubAppPath, kSubAppPath}, {kSubAppPath2, kSubAppPath2}});

  EXPECT_EQ(2ul, GetAllSubAppIds(parent_app_id_).size());

  std::vector<std::pair<webapps::ManifestId, SubAppsServiceResultCode>>
      expected_result;
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

  webapps::ManifestId sub_app_id_1 = GetURLFromPath(kSubAppPath);
  webapps::ManifestId sub_app_id_2 = GetURLFromPath(kSubAppPath2);
  EXPECT_FALSE(provider().registrar_unsafe().IsInstalled(
      GenerateAppIdFromManifestId(sub_app_id_1, parent_manifest_id())));
  EXPECT_FALSE(provider().registrar_unsafe().IsInstalled(
      GenerateAppIdFromManifestId(sub_app_id_2, parent_manifest_id())));

  std::optional<message_center::Notification> uninstall_notification =
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
  content::RenderFrameHost* iwa_frame = InstallAndOpenParentIwaApp();
  BindRemote(iwa_frame);

  webapps::AppId sub_app_id =
      GenerateSubAppIdFromPath(kSubAppPath, iwa_frame, parent_manifest_id());

  ExpectCallAdd(
      {{GetURLFromPath(kSubAppPath), SubAppsServiceResultCode::kSuccess}},
      {{kSubAppPath, kSubAppPath}});
  EXPECT_EQ(1ul, GetAllSubAppIds(parent_app_id_).size());
  EXPECT_TRUE(provider().registrar_unsafe().IsInstalled(sub_app_id));

  CallRemove({});
  EXPECT_EQ(1ul, GetAllSubAppIds(parent_app_id_).size());
  EXPECT_TRUE(provider().registrar_unsafe().IsInstalled(sub_app_id));
  EXPECT_FALSE(UninstallNotificationShown());
}

// Remove fails for a regular installed app.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, RemoveFailRegularApp) {
  // Regular install.
  InstallPwaFromPath(kSubAppPath);

  content::RenderFrameHost* iwa_frame = InstallAndOpenParentIwaApp();
  BindRemote(iwa_frame);

  EXPECT_EQ(
      SingleRemoveResultMojo(kSubAppPath, SubAppsServiceResultCode::kFailure),
      CallRemove({kSubAppPath}));
  EXPECT_FALSE(UninstallNotificationShown());
}

// Remove fails for a sub-app with a different parent_app_id.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, RemoveFailWrongParent) {
  content::RenderFrameHost* iwa_frame = InstallAndOpenParentIwaApp();
  BindRemote(iwa_frame);

  ExpectCallAdd(
      {{GetURLFromPath(kSubAppPath), SubAppsServiceResultCode::kSuccess}},
      {{kSubAppPath, kSubAppPath}});

  // Install a second IWA.
  auto iwa_dev_server_2 = CreateAndStartDevServer(
      FILE_PATH_LITERAL("web_apps/simple_isolated_app"));
  IsolatedWebAppUrlInfo parent_app_2 =
      web_app::InstallDevModeProxyIsolatedWebApp(browser()->profile(),
                                                 iwa_dev_server_->GetOrigin());
  content::RenderFrameHost* iwa_frame_2 = OpenApp(parent_app_2.app_id());
  remote_.reset();
  BindRemote(iwa_frame_2);

  EXPECT_EQ(
      SingleRemoveResultMojo(kSubAppPath2, SubAppsServiceResultCode::kFailure),
      CallRemove({kSubAppPath2}));
  EXPECT_FALSE(UninstallNotificationShown());
}

// Remove call returns failure if the calling app isn't installed.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest,
                       RemoveFailCallingAppNotInstalled) {
  content::RenderFrameHost* iwa_frame = InstallAndOpenParentIwaApp();
  BindRemote(iwa_frame);

  // In order to trigger the edge case of the parent app being uninstalled in
  // between an API call making it to the backend, just remove it from the
  // database here, allowing the parent window to remain active to make the
  // API call.
  {
    auto* sync_bridge = &provider().sync_bridge_unsafe();
    auto update = sync_bridge->BeginUpdate();
    update->DeleteApp(parent_app_id_);
  }

  EXPECT_EQ(
      SingleRemoveResultMojo(kSubAppPath, SubAppsServiceResultCode::kFailure),
      CallRemove({kSubAppPath}));
  EXPECT_FALSE(UninstallNotificationShown());
}

// Remove call closes the mojo connection if the argument is wrong origin to the
// calling app.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, RemoveFailWrongOrigin) {
  content::RenderFrameHost* iwa_frame = InstallAndOpenParentIwaApp();
  BindRemote(iwa_frame);

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
