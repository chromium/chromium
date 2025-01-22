// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/files/file_util.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/ui/web_applications/sub_apps_service_impl.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/commands/install_isolated_web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_install_source.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_trust_checker.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/test_signed_web_bundle_builder.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/blink/public/common/features.h"

using InstallResult =
    base::expected<web_app::InstallIsolatedWebAppCommandSuccess,
                   web_app::InstallIsolatedWebAppCommandError>;

namespace web_app {

class SubAppsPermissionsPolicyBrowserTest
    : public IsolatedWebAppBrowserTestHarness {
  base::ScopedTempDir scoped_temp_dir_;

 protected:
  base::test::ScopedFeatureList features_{blink::features::kDesktopPWAsSubApps};

 public:
  webapps::AppId parent_app_id_;

  void InstallIwaApp() {
    IsolatedWebAppUrlInfo url_info =
        *web_app::IsolatedWebAppBuilder(web_app::ManifestBuilder()
                                            .SetName("Parent apps test app")
                                            .SetVersion("1.0.0"))
             .BuildBundle()
             ->Install(profile());

    parent_app_id_ = url_info.app_id();

    auto installed_version = base::Version("1.0.0");

    std::unique_ptr<ScopedBundledIsolatedWebApp> bundle =
        IsolatedWebAppBuilder(ManifestBuilder()
                                  .SetName("Sub apps test app")
                                  .SetVersion(installed_version.GetString()))
            .BuildBundle();

    *bundle->Install(profile());

    const WebApp* web_app =
        provider().registrar_unsafe().GetAppById(parent_app_id_);

    EXPECT_EQ(proto::InstallState::INSTALLED_WITH_OS_INTEGRATION,
              provider().registrar_unsafe().GetInstallState(parent_app_id_));

    EXPECT_TRUE(provider().registrar_unsafe().IsIsolated(parent_app_id_));

    ASSERT_THAT(web_app, Pointee(test::Property(
                             "untranslated_name", &WebApp::untranslated_name,
                             testing::Eq("Parent apps test app"))));
  }
};

IN_PROC_BROWSER_TEST_F(SubAppsPermissionsPolicyBrowserTest,
                       EndToEndAddFailsIfPermissionsPolicyIsMissing) {
  ASSERT_NO_FATAL_FAILURE(InstallIwaApp());
  content::RenderFrameHost* iwa_frame = OpenApp(parent_app_id_);

  auto result = content::ExecJs(iwa_frame, "navigator.subApps.add({})");
  EXPECT_FALSE(result);
  std::string message = result.message();
  EXPECT_NE(message.find(
                "The executing top-level browsing context is not granted the "
                "\"sub-apps\" permissions policy."),
            std::string::npos);

  EXPECT_THAT(provider().registrar_unsafe().GetAllSubAppIds(parent_app_id_),
              testing::IsEmpty());
}

}  // namespace web_app
