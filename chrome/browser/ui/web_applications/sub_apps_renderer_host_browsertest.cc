// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "chrome/browser/ui/web_applications/sub_apps_renderer_host.h"

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

using blink::mojom::SubAppsProviderResult;

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
constexpr const char kSubAppPathInvalid[] = "/invalid/sub/app/path.html";

}  // namespace

// There's one simple end-to-end test that actually calls the JS API interface,
// the rest test the mojo interface (since the first layer listening to the API
// calls is almost a direct passthrough to the mojo service).
// TODO(isandrk): JS API interface tests should be in
// third_party/blink/web_tests/wpt_internal/subapps/.

class SubAppsRendererHostBrowserTest : public WebAppControllerBrowserTest {
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

  std::vector<AppId> GetAllSubAppIds(const AppId& parent_app_id) {
    return provider().registrar().GetAllSubAppIds(parent_app_id);
  }

  void BindRemote(content::WebContents* web_contents = nullptr) {
    // Any navigation causes the remote to be destroyed (since the
    // render_frame_host that owns it gets destroyed.)
    SubAppsRendererHost::CreateIfAllowed(render_frame_host(web_contents),
                                         remote_.BindNewPipeAndPassReceiver());
  }

  // Calls the Add() method on the mojo interface which is async, and waits for
  // it to finish.
  SubAppsProviderResult CallAdd(const std::string& install_path) {
    base::test::TestFuture<SubAppsProviderResult> future;
    remote_->Add(install_path, future.GetCallback());
    return future.Get();
  }

 protected:
  base::test::ScopedFeatureList features_{blink::features::kDesktopPWAsSubApps};
  AppId parent_app_id_;
  mojo::Remote<blink::mojom::SubAppsProvider> remote_;
};

// Simple end-to-end test for add().
IN_PROC_BROWSER_TEST_F(SubAppsRendererHostBrowserTest, EndToEndAdd) {
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
IN_PROC_BROWSER_TEST_F(SubAppsRendererHostBrowserTest,
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
IN_PROC_BROWSER_TEST_F(SubAppsRendererHostBrowserTest, AddSingle) {
  // Dependency graph:
  // NavigateToParentApp --> BindRemote --> CallAdd
  //                   \---------------->/
  // InstallParentApp ----------------->/
  NavigateToParentApp();
  BindRemote();
  InstallParentApp();
  EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id_).size());
  EXPECT_EQ(SubAppsProviderResult::kSuccess, CallAdd(kSubAppPath));

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
IN_PROC_BROWSER_TEST_F(SubAppsRendererHostBrowserTest, AddTwo) {
  InstallParentApp();
  NavigateToParentApp();
  BindRemote();

  EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id_).size());

  EXPECT_EQ(SubAppsProviderResult::kSuccess, CallAdd(kSubAppPath));
  EXPECT_EQ(1ul, GetAllSubAppIds(parent_app_id_).size());

  EXPECT_EQ(SubAppsProviderResult::kSuccess, CallAdd(kSubAppPath));
  EXPECT_EQ(1ul, GetAllSubAppIds(parent_app_id_).size());

  EXPECT_EQ(SubAppsProviderResult::kSuccess, CallAdd(kSubAppPath2));
  EXPECT_EQ(2ul, GetAllSubAppIds(parent_app_id_).size());
}

// Add call should fail if the parent app isn't installed.
IN_PROC_BROWSER_TEST_F(SubAppsRendererHostBrowserTest,
                       AddFailParentAppNotInstalled) {
  NavigateToParentApp();
  BindRemote();
  EXPECT_EQ(SubAppsProviderResult::kFailure, CallAdd(kSubAppPath));
}

// Add call should fail if the call wasn't made from the context of parent app.
IN_PROC_BROWSER_TEST_F(SubAppsRendererHostBrowserTest,
                       AddFailNotInParentAppContext) {
  InstallParentApp();
  BindRemote();
  EXPECT_EQ(SubAppsProviderResult::kFailure, CallAdd(kSubAppPath));
}

// Make sure the Add API can't force manifest update. Add sub-app, verify
// display mode, then add the same one again with different display mode in the
// manifest, and verify that it didn't change.
IN_PROC_BROWSER_TEST_F(SubAppsRendererHostBrowserTest,
                       AddDoesntForceReinstall) {
  InstallParentApp();
  NavigateToParentApp();
  BindRemote();
  EXPECT_EQ(SubAppsProviderResult::kSuccess, CallAdd(kSubAppPath));

  AppId sub_app_id =
      GenerateAppId(/*manifest_id=*/absl::nullopt, GetURL(kSubAppPath));
  EXPECT_EQ(DisplayMode::kStandalone,
            provider().registrar().GetAppEffectiveDisplayMode(sub_app_id));

  EXPECT_EQ(SubAppsProviderResult::kSuccess, CallAdd(kSubAppPathMinimalUi));
  EXPECT_EQ(DisplayMode::kStandalone,
            provider().registrar().GetAppEffectiveDisplayMode(sub_app_id));
}

// Verify that Add works if PWA is launched as standalone window.
IN_PROC_BROWSER_TEST_F(SubAppsRendererHostBrowserTest, AddStandaloneWindow) {
  InstallParentApp();
  content::WebContents* web_contents = OpenApplication(parent_app_id_);
  BindRemote(web_contents);
  EXPECT_EQ(SubAppsProviderResult::kSuccess, CallAdd(kSubAppPath));
}

// Verify that Add fails for an invalid (non-existing) sub-app.
IN_PROC_BROWSER_TEST_F(SubAppsRendererHostBrowserTest, AddInvalid) {
  InstallParentApp();
  NavigateToParentApp();
  BindRemote();

  EXPECT_EQ(SubAppsProviderResult::kFailure, CallAdd(kSubAppPathInvalid));
  EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id_).size());
}

}  // namespace web_app
