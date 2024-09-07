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
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_install_source.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_trust_checker.h"
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
  base::FilePath bundle_path_;
  web_package::test::Ed25519KeyPair key_pair_ =
      web_package::test::Ed25519KeyPair::CreateRandom();

  TestSignedWebBundle CreateBundle() const {
    constexpr std::string_view manifest =
        R"({
          "name": "Sub apps test app",
          "id": "/",
          "version": "1.0.0",
          "scope": "/",
          "short_name": "Sub apps app",
          "start_url": "/index.html",
          "display": "standalone",
          "background_color": "#0000FF",
          "theme_color": "#000077",
          "icons": [
            {
              "src": "/256x256-green.png",
              "sizes": "256x256",
              "type": "image/png"
            }
          ],
          "permissions_policy": {
            "cross-origin-isolated": ["self"]
          }
        })";

    auto builder = TestSignedWebBundleBuilder(key_pair_);
    builder.AddManifest(manifest);

    builder.AddPngImage(
        "/256x256-green.png",
        test::EncodeAsPng(CreateSquareIcon(256, SK_ColorGREEN)));

    builder.AddHtml("/index.html", R"(
      <head>
        <link rel="manifest" href="/manifest.webmanifest">
        <title>Test App</title>
      </head>
      <body>
        <h1>Hello world!</h1>
      </body>
    )");

    return builder.Build();
  }

  void WriteBundle(TestSignedWebBundle bundle) {
    ASSERT_THAT(scoped_temp_dir_.CreateUniqueTempDir(), testing::IsTrue());
    bundle_path_ = scoped_temp_dir_.GetPath().Append(
        base::FilePath::FromASCII("bundle.swbn"));
    CHECK(base::WriteFile(bundle_path_, bundle.data));
  }

 protected:
  base::test::ScopedFeatureList features_{blink::features::kDesktopPWAsSubApps};

 public:
  webapps::AppId parent_app_id_;

  void SetUp() override {
    auto bundle = CreateBundle();
    WriteBundle(bundle);

    IsolatedWebAppBrowserTestHarness::SetUp();
  }

  void InstallIwaApp() {
    auto install_source = IsolatedWebAppInstallSource::FromGraphicalInstaller(
        IwaSourceBundleProdModeWithFileOp(bundle_path_,
                                          IwaSourceBundleProdFileOp::kCopy));

    IsolatedWebAppUrlInfo url_info =
        IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
            web_package::SignedWebBundleId::CreateForPublicKey(
                key_pair_.public_key));

    parent_app_id_ = url_info.app_id();

    base::test::TestFuture<InstallResult> future;
    auto installed_version = base::Version("1.0.0");

    SetTrustedWebBundleIdsForTesting({url_info.web_bundle_id()});
    provider().scheduler().InstallIsolatedWebApp(
        url_info, install_source, installed_version,
        /*optional_keep_alive*/ nullptr,
        /*optional_profile_keep_alive*/ nullptr, future.GetCallback());

    ASSERT_THAT(future.Take(), base::test::HasValue());

    const WebApp* web_app =
        provider().registrar_unsafe().GetAppById(parent_app_id_);

    EXPECT_TRUE(provider().registrar_unsafe().IsInstalled(parent_app_id_));
    EXPECT_TRUE(provider().registrar_unsafe().IsIsolated(parent_app_id_));

    ASSERT_THAT(web_app, Pointee(test::Property(
                             "untranslated_name", &WebApp::untranslated_name,
                             testing::Eq("Sub apps test app"))));
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
