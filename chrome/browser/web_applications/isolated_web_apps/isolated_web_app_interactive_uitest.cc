// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string_view>

#include "base/files/file_path.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/run_until.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "components/permissions/permission_request_manager.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace web_app {

namespace {

using IsolatedWebAppBrowserTest = IsolatedWebAppBrowserTestHarness;

IN_PROC_BROWSER_TEST_F(IsolatedWebAppBrowserTest, ClipboardReadWrite) {
  std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      IsolatedWebAppBuilder(
          ManifestBuilder()
              .AddPermissionsPolicyWildcard(
                  network::mojom::PermissionsPolicyFeature::kClipboardRead)
              .AddPermissionsPolicyWildcard(
                  network::mojom::PermissionsPolicyFeature::kClipboardWrite))
          .BuildBundle();
  ASSERT_OK_AND_ASSIGN(web_app::IsolatedWebAppUrlInfo url_info,
                       app->Install(profile()));
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());
  content::WebContents* app_web_contents =
      content::WebContents::FromRenderFrameHost(app_frame);

  // Automatically allow clipboard permissions.
  auto* permission_request_manager =
      permissions::PermissionRequestManager::FromWebContents(app_web_contents);
  permission_request_manager->set_auto_response_for_test(
      permissions::PermissionRequestManager::ACCEPT_ALL);

  app_frame->GetView()->Focus();
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return app_frame->GetView()->HasFocus(); }));

  constexpr std::string_view kClipboardData = "Isolated Web App";
  EXPECT_TRUE(ExecJs(
      app_frame,
      content::JsReplace("navigator.clipboard.writeText($1)", kClipboardData)));
  EXPECT_EQ(kClipboardData,
            EvalJs(app_frame, "navigator.clipboard.readText()"));
}

}  // namespace

}  // namespace web_app
