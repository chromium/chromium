// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "chrome/browser/ui/web_applications/sub_apps_service_impl.h"

#include "base/containers/flat_set.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/features.h"

using blink::mojom::SubAppsServiceListResultPtr;
using blink::mojom::SubAppsServiceResult;

namespace web_app {

namespace {

constexpr const char kDomain[] = "www.foo.bar";
constexpr const char kSubDomain[] = "baz.foo.bar";
constexpr const char kDifferentDomain[] = "www.different-domain.com";
constexpr const char kParentAppPath[] = "/web_apps/basic.html";
constexpr const char kSubAppPath[] = "/web_apps/site_a/basic.html";
constexpr const char kSubAppPathMinimalUi[] =
    "/web_apps/site_a/basic.html?manifest=manifest_minimal_ui.json";
constexpr const char kSubAppPath2[] = "/web_apps/site_b/basic.html";
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
    return web_contents->GetMainFrame();
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
  SubAppsServiceResult CallAdd(const std::string& install_path) {
    base::test::TestFuture<SubAppsServiceResult> future;
    remote_->Add(install_path, future.GetCallback());
    return future.Get();
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

 protected:
  base::test::ScopedFeatureList features_{blink::features::kDesktopPWAsSubApps};
  AppId parent_app_id_;
  mojo::Remote<blink::mojom::SubAppsService> remote_;
};

// Simple end-to-end test for add().
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, EndToEndAdd) {
  InstallParentApp();
  NavigateToParentApp();
  EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id_).size());

  // Relative path (/path/to/app).
  EXPECT_TRUE(
      ExecJs(render_frame_host(),
             content::JsReplace("navigator.subApps.add($1)", kSubAppPath)));
  // ExecJs waits until the Promise returned by add() resolves.
  EXPECT_EQ(1ul, GetAllSubAppIds(parent_app_id_).size());

  // Full URL (https://sub.domain.org/path/to/app).
  EXPECT_TRUE(ExecJs(
      render_frame_host(),
      content::JsReplace("navigator.subApps.add($1)", GetURL(kSubAppPath2))));
  EXPECT_EQ(2ul, GetAllSubAppIds(parent_app_id_).size());
}

// End-to-end. Test that adding a sub-app from a different origin or from a
// different domain fails.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest,
                       EndToEndAddFailDifferentOrigin) {
  InstallParentApp();
  NavigateToParentApp();
  EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id_).size());

  GURL different_origin = https_server()->GetURL(kSubDomain, kSubAppPath);
  // EXPECT_FALSE because this returns an error.
  EXPECT_FALSE(ExecJs(
      render_frame_host(),
      content::JsReplace("navigator.subApps.add($1)", different_origin)));
  EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id_).size());

  GURL different_domain =
      https_server()->GetURL(kDifferentDomain, kSubAppPath2);
  EXPECT_FALSE(ExecJs(
      render_frame_host(),
      content::JsReplace("navigator.subApps.add($1)", different_domain)));
  EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id_).size());
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
  EXPECT_EQ(SubAppsServiceResult::kSuccess, CallAdd(kSubAppPath));

  // Verify a bunch of things for the newly installed sub-app.
  AppId sub_app_id =
      GenerateAppId(/*manifest_id=*/absl::nullopt, GetURL(kSubAppPath));
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
  EXPECT_EQ(sub_app->start_url(), GetURL(kSubAppPath));
  if (provider().ui_manager().CanAddAppToQuickLaunchBar()) {
    EXPECT_FALSE(provider().ui_manager().IsAppInQuickLaunchBar(sub_app_id));
  }
}

// Add one sub-app, verify count is one. Add it again, still same count. Add a
// second sub-app, verify count is two.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, AddTwo) {
  InstallParentApp();
  NavigateToParentApp();
  BindRemote();

  EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id_).size());

  EXPECT_EQ(SubAppsServiceResult::kSuccess, CallAdd(kSubAppPath));
  EXPECT_EQ(1ul, GetAllSubAppIds(parent_app_id_).size());

  EXPECT_EQ(SubAppsServiceResult::kSuccess, CallAdd(kSubAppPath));
  EXPECT_EQ(1ul, GetAllSubAppIds(parent_app_id_).size());

  EXPECT_EQ(SubAppsServiceResult::kSuccess, CallAdd(kSubAppPath2));
  EXPECT_EQ(2ul, GetAllSubAppIds(parent_app_id_).size());
}

// Add call should fail if the parent app isn't installed.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest,
                       AddFailParentAppNotInstalled) {
  NavigateToParentApp();
  BindRemote();
  EXPECT_EQ(SubAppsServiceResult::kFailure, CallAdd(kSubAppPath));
}

// Add call should fail if the call wasn't made from the context of parent app.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest,
                       AddFailNotInParentAppContext) {
  InstallParentApp();
  BindRemote();
  EXPECT_EQ(SubAppsServiceResult::kFailure, CallAdd(kSubAppPath));
}

// Make sure the Add API can't force manifest update. Add sub-app, verify
// display mode, then add the same one again with different display mode in the
// manifest, and verify that it didn't change.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, AddDoesntForceReinstall) {
  InstallParentApp();
  NavigateToParentApp();
  BindRemote();
  EXPECT_EQ(SubAppsServiceResult::kSuccess, CallAdd(kSubAppPath));

  AppId sub_app_id =
      GenerateAppId(/*manifest_id=*/absl::nullopt, GetURL(kSubAppPath));
  EXPECT_EQ(DisplayMode::kStandalone,
            provider().registrar().GetAppEffectiveDisplayMode(sub_app_id));

  EXPECT_EQ(SubAppsServiceResult::kSuccess, CallAdd(kSubAppPathMinimalUi));
  EXPECT_EQ(DisplayMode::kStandalone,
            provider().registrar().GetAppEffectiveDisplayMode(sub_app_id));
}

// Verify that Add works if PWA is launched as standalone window.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, AddStandaloneWindow) {
  InstallParentApp();
  content::WebContents* web_contents = OpenApplication(parent_app_id_);
  BindRemote(web_contents);
  EXPECT_EQ(SubAppsServiceResult::kSuccess, CallAdd(kSubAppPath));
}

// Verify that Add fails for an invalid (non-existing) sub-app.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, AddInvalid) {
  InstallParentApp();
  NavigateToParentApp();
  BindRemote();

  EXPECT_EQ(SubAppsServiceResult::kFailure, CallAdd(kSubAppPathInvalid));
  EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id_).size());
}

// Verify that uninstalling an app with sub-apps causes sub-apps to be
// uninstalled as well.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest,
                       UninstallingParentAppUninstallsSubApps) {
  InstallParentApp();
  NavigateToParentApp();
  BindRemote();

  EXPECT_EQ(SubAppsServiceResult::kSuccess, CallAdd(kSubAppPath));
  EXPECT_EQ(SubAppsServiceResult::kSuccess, CallAdd(kSubAppPath2));
  EXPECT_EQ(SubAppsServiceResult::kSuccess, CallAdd(kSubAppPath3));

  // Verify that subapps are installed.
  AppId sub_app_id_1 =
      GenerateAppId(/*manifest_id=*/absl::nullopt, GetURL(kSubAppPath));
  AppId sub_app_id_2 =
      GenerateAppId(/*manifest_id=*/absl::nullopt, GetURL(kSubAppPath2));
  AppId sub_app_id_3 =
      GenerateAppId(/*manifest_id=*/absl::nullopt, GetURL(kSubAppPath3));

  EXPECT_TRUE(provider().registrar().IsInstalled(sub_app_id_1));
  EXPECT_TRUE(provider().registrar().IsInstalled(sub_app_id_2));
  EXPECT_TRUE(provider().registrar().IsInstalled(sub_app_id_3));

  UninstallParentApp();
  // Verify that both parent app and sub apps are no longer installed.
  EXPECT_FALSE(provider().registrar().IsInstalled(parent_app_id_));
  EXPECT_FALSE(provider().registrar().IsInstalled(sub_app_id_1));
  EXPECT_FALSE(provider().registrar().IsInstalled(sub_app_id_2));
  EXPECT_FALSE(provider().registrar().IsInstalled(sub_app_id_3));
}

// Verify that uninstalling an app that has a sub-app with more than one
// install source only removes the "sub-app" install source for that sub-app
// but does not uninstall it.
IN_PROC_BROWSER_TEST_F(
    SubAppsServiceImplBrowserTest,
    StandaloneAppStaysInstalledAfterUpgradedParentUninstall) {
  // Install app as standalone app.
  AppId standalone_app_id = InstallPWA(GetURL(kSubAppPath2));

  InstallParentApp();
  NavigateToParentApp();
  BindRemote();

  // Add normal subapp to verify standalone app install/uninstall does
  // not affect normal sub app uninstalls.
  EXPECT_EQ(SubAppsServiceResult::kSuccess, CallAdd(kSubAppPath));
  AppId sub_app_id =
      GenerateAppId(/*manifest_id=*/absl::nullopt, GetURL(kSubAppPath));
  EXPECT_TRUE(provider().registrar().IsInstalled(sub_app_id));

  // Add standalone app as sub-app.
  EXPECT_EQ(SubAppsServiceResult::kSuccess, CallAdd(kSubAppPath2));

  // Verify that it is now installed and registered as a subapp.
  const WebApp* standalone_app =
      provider().registrar().GetAppById(standalone_app_id);
  EXPECT_EQ(parent_app_id_, standalone_app->parent_app_id());
  EXPECT_FALSE(standalone_app->HasOnlySource(WebAppManagement::kSync));
  EXPECT_TRUE(standalone_app->IsSubAppInstalledApp());

  UninstallParentApp();

  // Verify that normal sub-app is uninstalled.
  EXPECT_FALSE(provider().registrar().IsInstalled(sub_app_id));

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
  InstallParentApp();
  NavigateToParentApp();
  BindRemote();

  // Empty list before adding any sub-apps.
  SubAppsServiceListResultPtr result = CallList();
  EXPECT_EQ(SubAppsServiceResult::kSuccess, result->code);
  EXPECT_EQ(std::vector<std::string>{}, result->sub_app_ids);

  EXPECT_EQ(SubAppsServiceResult::kSuccess, CallAdd(kSubAppPath));
  EXPECT_EQ(SubAppsServiceResult::kSuccess, CallAdd(kSubAppPath2));
  EXPECT_EQ(SubAppsServiceResult::kSuccess, CallAdd(kSubAppPath3));

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

  InstallParentApp();
  NavigateToParentApp();
  BindRemote();

  // Sub-app install.
  EXPECT_EQ(SubAppsServiceResult::kSuccess, CallAdd(kSubAppPath2));

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

  std::string unhashed_app_id = GetURL(kSubAppPath).spec();
  AppId app_id = GenerateAppIdFromUnhashed(unhashed_app_id);

  EXPECT_EQ(SubAppsServiceResult::kSuccess, CallAdd(kSubAppPath));
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

  std::string unhashed_app_id = GetURL(kSubAppPath).spec();
  EXPECT_EQ(SubAppsServiceResult::kFailure, CallRemove(unhashed_app_id));
}

// Remove fails for a sub-app with a different parent_app_id.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest, RemoveFailWrongParent) {
  // SubApp plays the parent app here, SubApp2 is its sub-app, SubApp3 is the
  // other "parent app".
  AppId parent_app = InstallPWA(GetURL(kSubAppPath));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetURL(kSubAppPath)));
  BindRemote();

  EXPECT_EQ(SubAppsServiceResult::kSuccess, CallAdd(kSubAppPath2));

  AppId second_parent_app = InstallPWA(GetURL(kSubAppPath3));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetURL(kSubAppPath3)));
  remote_.reset();
  BindRemote();

  std::string unhashed_app_id = GetURL(kSubAppPath2).spec();
  EXPECT_EQ(SubAppsServiceResult::kFailure, CallRemove(unhashed_app_id));
}

// Remove call returns failure if the calling app isn't installed.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest,
                       RemoveFailCallingAppNotInstalled) {
  NavigateToParentApp();
  BindRemote();

  std::string unhashed_app_id = GetURL(kSubAppPath).spec();
  EXPECT_EQ(SubAppsServiceResult::kFailure, CallRemove(unhashed_app_id));
}

// Remove doesn't crash with an invalid unhashed_app_id.
IN_PROC_BROWSER_TEST_F(SubAppsServiceImplBrowserTest,
                       RemoveInvalidArgDoesntCrash) {
  InstallParentApp();
  NavigateToParentApp();
  BindRemote();

  // Invalid because it isn't a proper URL.
  std::string unhashed_app_id = "invalid";
  EXPECT_EQ(SubAppsServiceResult::kFailure, CallRemove(unhashed_app_id));

  // Shouldn't crash.
}

}  // namespace web_app
