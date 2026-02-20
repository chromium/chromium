// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include "base/check_deref.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/strings/string_util.h"
#include "base/task/task_traits.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "base/test/with_feature_override.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/ui/webui/web_app_internals/web_app_internals.mojom.h"
#include "chrome/browser/ui/webui/web_app_internals/web_app_internals_handler.h"
#include "chrome/browser/ui/webui/web_app_internals/web_app_internals_ui.h"
#include "chrome/browser/web_applications/isolated_web_apps/commands/isolated_web_app_install_command_helper.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_test_update_server.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/isolated_web_apps/test_support/signing_keys.h"
#include "components/webapps/isolated_web_apps/types/update_channel.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::HasSubstr;
using ::testing::Pointee;

}  // namespace

class WebAppInternalsIwaInstallationBrowserTest
    : public IsolatedWebAppBrowserTestHarness {
 protected:
  WebAppInternalsHandler* OpenWebAppInternals() {
    EXPECT_TRUE(ui_test_utils::NavigateToURL(
        browser(), GURL("chrome://web-app-internals")));
    return static_cast<WebAppInternalsUI*>(browser()
                                               ->tab_strip_model()
                                               ->GetActiveWebContents()
                                               ->GetWebUI()
                                               ->GetController())
        ->GetHandlerForTesting();
  }

  IsolatedWebAppTestUpdateServer iwa_test_update_server_;
};

IN_PROC_BROWSER_TEST_F(WebAppInternalsIwaInstallationBrowserTest,
                       FetchUpdateManifestAndInstallIwaAndUpdate) {
  iwa_test_update_server_.AddBundle(
      IsolatedWebAppBuilder(ManifestBuilder().SetVersion("1.0.0"))
          .BuildBundle(test::GetDefaultEd25519KeyPair()));

  auto* handler = OpenWebAppInternals();

  GURL update_manifest_url = iwa_test_update_server_.GetUpdateManifestUrl(
      test::GetDefaultEd25519WebBundleId());
  base::test::TestFuture<::mojom::ParseUpdateManifestFromUrlResultPtr>
      um_future;
  handler->ParseUpdateManifestFromUrl(update_manifest_url,
                                      um_future.GetCallback());

  auto um_result = um_future.Take();
  ASSERT_TRUE(um_result->is_update_manifest());

  const auto& update_manifest = *um_result->get_update_manifest();

  ASSERT_THAT(update_manifest,
              Field(&::mojom::UpdateManifest::versions,
                    ElementsAre(Pointee(
                        Field(&::mojom::VersionEntry::version, Eq("1.0.0"))))));

  const GURL& web_bundle_url = update_manifest.versions[0]->web_bundle_url;

  base::test::TestFuture<::mojom::InstallIsolatedWebAppResultPtr>
      install_future;
  auto params = ::mojom::InstallFromBundleUrlParams::New();
  params->web_bundle_url = web_bundle_url;
  params->update_info = ::mojom::UpdateInfo::New(
      update_manifest_url, UpdateChannel::default_channel().ToString(),
      /*pinned_version=*/std::nullopt, /*allow_downgrades=*/false);
  handler->InstallIsolatedWebAppFromBundleUrl(std::move(params),
                                              install_future.GetCallback());
  ASSERT_TRUE(install_future.Take()->is_success());

  webapps::AppId app_id = IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
                              test::GetDefaultEd25519WebBundleId())
                              .app_id();
  {
    const WebApp& iwa = CHECK_DEREF(provider().registrar_unsafe().GetAppById(
        app_id, WebAppFilter::IsIsolatedApp()));
    EXPECT_EQ(iwa.isolation_data()->version(), *IwaVersion::Create("1.0.0"));
    EXPECT_EQ(iwa.isolation_data()->update_manifest_url(), update_manifest_url);
    EXPECT_EQ(iwa.isolation_data()->update_channel(),
              UpdateChannel::default_channel());
  }

  // Run an update check on the same manifest.
  {
    base::test::TestFuture<std::string> update_future;
    handler->UpdateManifestInstalledIsolatedWebApp(
        app_id, update_future.GetCallback<const std::string&>());
    EXPECT_THAT(update_future.Get(),
                HasSubstr("app is already on the latest version"));
  }

  // Now add a new entry to the manifest and re-run the update check.
  iwa_test_update_server_.AddBundle(
      IsolatedWebAppBuilder(ManifestBuilder().SetVersion("2.0.0"))
          .BuildBundle(test::GetDefaultEd25519KeyPair()));
  {
    base::test::TestFuture<std::string> update_future;
    handler->UpdateManifestInstalledIsolatedWebApp(
        app_id, update_future.GetCallback<const std::string&>());
    EXPECT_THAT(update_future.Get(), HasSubstr("Update to v2.0.0 successful"));

    const WebApp& iwa = CHECK_DEREF(provider().registrar_unsafe().GetAppById(
        app_id, WebAppFilter::IsIsolatedApp()));
    EXPECT_EQ(iwa.isolation_data()->version(), *IwaVersion::Create("2.0.0"));
    EXPECT_EQ(iwa.isolation_data()->update_manifest_url(), update_manifest_url);
    EXPECT_EQ(iwa.isolation_data()->update_channel(),
              UpdateChannel::default_channel());
  }

  // Set the channel to "beta" and verify that other fields of IsolationData
  // stay intact.
  auto beta_channel = *UpdateChannel::Create("beta");
  {
    base::test::TestFuture<bool> set_channel_future;
    handler->SetUpdateChannelForIsolatedWebApp(
        app_id, beta_channel.ToString(), set_channel_future.GetCallback());
    EXPECT_TRUE(set_channel_future.Get());

    const WebApp& iwa = CHECK_DEREF(provider().registrar_unsafe().GetAppById(
        app_id, WebAppFilter::IsIsolatedApp()));
    EXPECT_EQ(iwa.isolation_data()->version(), *IwaVersion::Create("2.0.0"));
    EXPECT_EQ(iwa.isolation_data()->update_manifest_url(), update_manifest_url);
    EXPECT_EQ(iwa.isolation_data()->update_channel(), beta_channel);
  }

  // Now add new entries with v2.1.0 for the `beta` channel and v2.2.0 for the
  // `default` channel and force an update check.
  iwa_test_update_server_.AddBundle(
      IsolatedWebAppBuilder(ManifestBuilder().SetVersion("2.1.0"))
          .BuildBundle(test::GetDefaultEd25519KeyPair()),
      /*update_channels=*/{{beta_channel}});
  iwa_test_update_server_.AddBundle(
      IsolatedWebAppBuilder(ManifestBuilder().SetVersion("2.2.0"))
          .BuildBundle(test::GetDefaultEd25519KeyPair()));

  // The update logic must pick up the v2.1.0 for `beta` instead of a higher
  // v2.2.0 for `default`.
  {
    base::test::TestFuture<std::string> update_future;
    handler->UpdateManifestInstalledIsolatedWebApp(
        app_id, update_future.GetCallback<const std::string&>());
    EXPECT_THAT(update_future.Get(), HasSubstr("Update to v2.1.0 successful"));

    const WebApp& iwa = CHECK_DEREF(provider().registrar_unsafe().GetAppById(
        app_id, WebAppFilter::IsIsolatedApp()));
    EXPECT_EQ(iwa.isolation_data()->version(), *IwaVersion::Create("2.1.0"));
    EXPECT_EQ(iwa.isolation_data()->update_manifest_url(), update_manifest_url);
    EXPECT_EQ(iwa.isolation_data()->update_channel(), beta_channel);
  }

  // Add v2.3.0 to `beta` channel, pin the app to v2.1.0. Expect no update.
  {
    iwa_test_update_server_.AddBundle(
        IsolatedWebAppBuilder(ManifestBuilder().SetVersion("2.3.0"))
            .BuildBundle(test::GetDefaultEd25519KeyPair()),
        /*update_channels=*/{{beta_channel}});

    base::test::TestFuture<bool> set_pinned_version_future;
    handler->SetPinnedVersionForIsolatedWebApp(
        app_id, "2.1.0", set_pinned_version_future.GetCallback());
    EXPECT_TRUE(set_pinned_version_future.Get());

    base::test::TestFuture<std::string> update_future;
    handler->UpdateManifestInstalledIsolatedWebApp(
        app_id, update_future.GetCallback<const std::string&>());
    EXPECT_THAT(
        update_future.Get(),
        HasSubstr("Update skipped: app is already on the latest version or the "
                  "updates are disabled due to set `pinned_version` field."));
  }

  // Add v2.4.0 to `beta` channel, pin the app to v2.3.0. Expect an update to
  // v2.3.0.
  {
    iwa_test_update_server_.AddBundle(
        IsolatedWebAppBuilder(ManifestBuilder().SetVersion("2.4.0"))
            .BuildBundle(test::GetDefaultEd25519KeyPair()),
        /*update_channels=*/{{beta_channel}});

    base::test::TestFuture<bool> set_pinned_version_future;
    handler->SetPinnedVersionForIsolatedWebApp(
        app_id, "2.3.0", set_pinned_version_future.GetCallback());
    EXPECT_TRUE(set_pinned_version_future.Get());

    base::test::TestFuture<std::string> update_future;
    handler->UpdateManifestInstalledIsolatedWebApp(
        app_id, update_future.GetCallback<const std::string&>());
    EXPECT_THAT(update_future.Get(), HasSubstr("Update to v2.3.0 successful"));

    const WebApp& iwa = CHECK_DEREF(provider().registrar_unsafe().GetAppById(
        app_id, WebAppFilter::IsIsolatedApp()));
    EXPECT_EQ(iwa.isolation_data()->version(), *IwaVersion::Create("2.3.0"));
    EXPECT_EQ(iwa.isolation_data()->update_manifest_url(), update_manifest_url);
    EXPECT_EQ(iwa.isolation_data()->update_channel(), beta_channel);
  }

  // Unpin the app. App should be updated to v2.4.0.
  {
    handler->ResetPinnedVersionForIsolatedWebApp(app_id);
    base::test::TestFuture<std::string> update_future;
    handler->UpdateManifestInstalledIsolatedWebApp(
        app_id, update_future.GetCallback<const std::string&>());
    EXPECT_THAT(update_future.Get(), HasSubstr("Update to v2.4.0 successful"));

    const WebApp& iwa = CHECK_DEREF(provider().registrar_unsafe().GetAppById(
        app_id, WebAppFilter::IsIsolatedApp()));
    EXPECT_EQ(iwa.isolation_data()->version(), *IwaVersion::Create("2.4.0"));
  }

  // Pin to v2.3.0, allow downgrades. Expect update to v2.3.0.
  {
    handler->SetAllowDowngradesForIsolatedWebApp(true, app_id);
    base::test::TestFuture<bool> set_pinned_version_future;
    handler->SetPinnedVersionForIsolatedWebApp(
        app_id, "2.3.0", set_pinned_version_future.GetCallback());
    EXPECT_TRUE(set_pinned_version_future.Get());

    base::test::TestFuture<std::string> update_future;
    handler->UpdateManifestInstalledIsolatedWebApp(
        app_id, update_future.GetCallback<const std::string&>());
    EXPECT_THAT(update_future.Get(), HasSubstr("Update to v2.3.0 successful"));

    const WebApp& iwa = CHECK_DEREF(provider().registrar_unsafe().GetAppById(
        app_id, WebAppFilter::IsIsolatedApp()));
    EXPECT_EQ(iwa.isolation_data()->version(), *IwaVersion::Create("2.3.0"));
  }
}

IN_PROC_BROWSER_TEST_F(
    WebAppInternalsIwaInstallationBrowserTest,
    FetchUpdateManifestAndInstallIwaAndUpdateInvalidPinnedVersion) {
  iwa_test_update_server_.AddBundle(
      IsolatedWebAppBuilder(ManifestBuilder().SetVersion("1.0.0"))
          .BuildBundle(test::GetDefaultEd25519KeyPair()));

  auto* handler = OpenWebAppInternals();

  GURL update_manifest_url = iwa_test_update_server_.GetUpdateManifestUrl(
      test::GetDefaultEd25519WebBundleId());
  base::test::TestFuture<::mojom::ParseUpdateManifestFromUrlResultPtr>
      um_future;
  handler->ParseUpdateManifestFromUrl(update_manifest_url,
                                      um_future.GetCallback());

  auto um_result = um_future.Take();
  ASSERT_TRUE(um_result->is_update_manifest());

  const auto& update_manifest = *um_result->get_update_manifest();

  ASSERT_THAT(update_manifest,
              Field(&::mojom::UpdateManifest::versions,
                    ElementsAre(Pointee(
                        Field(&::mojom::VersionEntry::version, Eq("1.0.0"))))));

  const GURL& web_bundle_url = update_manifest.versions[0]->web_bundle_url;

  base::test::TestFuture<::mojom::InstallIsolatedWebAppResultPtr>
      install_future;
  auto params = ::mojom::InstallFromBundleUrlParams::New();
  params->web_bundle_url = web_bundle_url;
  params->update_info = ::mojom::UpdateInfo::New(
      update_manifest_url, UpdateChannel::default_channel().ToString(),
      /*pinned_version=*/std::nullopt, /*allow_downgrades=*/false);
  handler->InstallIsolatedWebAppFromBundleUrl(std::move(params),
                                              install_future.GetCallback());
  ASSERT_TRUE(install_future.Take()->is_success());

  webapps::AppId app_id = IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
                              test::GetDefaultEd25519WebBundleId())
                              .app_id();

  // Add v2.0.0 to the update manifest.
  iwa_test_update_server_.AddBundle(
      IsolatedWebAppBuilder(ManifestBuilder().SetVersion("2.0.0"))
          .BuildBundle(test::GetDefaultEd25519KeyPair()));

  // Pin the app to an non-existent but valid version.
  {
    base::test::TestFuture<bool> set_pinned_version_future;
    handler->SetPinnedVersionForIsolatedWebApp(
        app_id, "1.1.1", set_pinned_version_future.GetCallback());
    EXPECT_TRUE(set_pinned_version_future.Get());

    base::test::TestFuture<std::string> update_future;
    handler->UpdateManifestInstalledIsolatedWebApp(
        app_id, update_future.GetCallback<const std::string&>());
    EXPECT_THAT(
        update_future.Get(),
        HasSubstr(
            "Update failed: Error::kPinnedVersionNotFoundInUpdateManifest"));

    const WebApp& iwa = CHECK_DEREF(provider().registrar_unsafe().GetAppById(
        app_id, WebAppFilter::IsIsolatedApp()));
    // Expect the app to stay at v1.0.0.
    EXPECT_EQ(iwa.isolation_data()->version(), *IwaVersion::Create("1.0.0"));
  }

  // Fails to pin the app to invalid version.
  {
    base::test::TestFuture<bool> set_pinned_version_future;
    handler->SetPinnedVersionForIsolatedWebApp(
        app_id, "invalid_version", set_pinned_version_future.GetCallback());
    EXPECT_THAT(set_pinned_version_future.Get(), 0);

    base::test::TestFuture<std::string> update_future;
    handler->UpdateManifestInstalledIsolatedWebApp(
        app_id, update_future.GetCallback<const std::string&>());
    EXPECT_THAT(
        update_future.Get(),
        HasSubstr(
            "Update failed: Error::kPinnedVersionNotFoundInUpdateManifest"));
  }
}

IN_PROC_BROWSER_TEST_F(WebAppInternalsIwaInstallationBrowserTest,
                       ParseUpdateManifestFromUrlFailsWithIncorrectUrl) {
  auto* handler = OpenWebAppInternals();

  base::test::TestFuture<::mojom::ParseUpdateManifestFromUrlResultPtr>
      um_future;

  // Select some dummy URL that certainly doesn't host an update manifest.
  handler->ParseUpdateManifestFromUrl(GURL("https://example.com"),
                                      um_future.GetCallback());
  ASSERT_TRUE(um_future.Take()->is_error());
}

IN_PROC_BROWSER_TEST_F(
    WebAppInternalsIwaInstallationBrowserTest,
    InstallIsolatedWebAppFromBundleUrlFailsWithIncorrectUrl) {
  auto* handler = OpenWebAppInternals();

  base::test::TestFuture<::mojom::InstallIsolatedWebAppResultPtr>
      install_future;
  auto params = ::mojom::InstallFromBundleUrlParams::New();

  // Select some dummy URL that certainly doesn't host a web bundle.
  params->web_bundle_url = GURL("https://example.com");
  handler->InstallIsolatedWebAppFromBundleUrl(std::move(params),
                                              install_future.GetCallback());
  ASSERT_TRUE(install_future.Take()->is_error());
}

// Tests the Isolated Web App deletion flow through the internals page handler.
IN_PROC_BROWSER_TEST_F(WebAppInternalsIwaInstallationBrowserTest,
                       DeleteIsolatedWebApp) {
  // Install a test IWA.
  auto bundle = IsolatedWebAppBuilder(ManifestBuilder()).BuildBundle();
  ASSERT_OK_AND_ASSIGN(auto url_info, bundle->Install(profile()));
  webapps::AppId app_id = url_info.app_id();

  EXPECT_NE(provider().registrar_unsafe().GetAppById(app_id), nullptr);

  auto* handler = OpenWebAppInternals();

  // Auto-accept the uninstall dialog.
  extensions::ScopedTestDialogAutoConfirm auto_accept(
      extensions::ScopedTestDialogAutoConfirm::ACCEPT);

  // Call the delete method on the handler.
  base::test::TestFuture<bool> delete_future;
  handler->DeleteIsolatedWebApp(app_id, delete_future.GetCallback());

  // Verify the deletion was successful and the app is no longer registered.
  EXPECT_TRUE(delete_future.Get());

  EXPECT_EQ(provider().registrar_unsafe().GetAppById(app_id), nullptr);
}

// Tests the Isolated Web App deletion flow through the internals page handler
// when the dialog box is cancelled.
IN_PROC_BROWSER_TEST_F(WebAppInternalsIwaInstallationBrowserTest,
                       DeleteIsolatedWebAppCancelled) {
  // Install a test IWA.
  auto bundle = IsolatedWebAppBuilder(ManifestBuilder()).BuildBundle();
  ASSERT_OK_AND_ASSIGN(auto url_info, bundle->Install(profile()));
  webapps::AppId app_id = url_info.app_id();

  EXPECT_NE(provider().registrar_unsafe().GetAppById(app_id), nullptr);

  auto* handler = OpenWebAppInternals();

  // Auto-cancel the uninstall dialog.
  extensions::ScopedTestDialogAutoConfirm auto_cancel(
      extensions::ScopedTestDialogAutoConfirm::CANCEL);

  // Call the delete method on the handler.
  base::test::TestFuture<bool> delete_future;
  handler->DeleteIsolatedWebApp(app_id, delete_future.GetCallback());

  // Verify the deletion was not successful because it was cancelled.
  EXPECT_FALSE(delete_future.Get());

  // Verify the app is still registered.
  EXPECT_NE(provider().registrar_unsafe().GetAppById(app_id), nullptr);
}

}  // namespace web_app
