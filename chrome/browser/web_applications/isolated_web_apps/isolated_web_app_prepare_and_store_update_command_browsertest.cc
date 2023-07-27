// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_prepare_and_store_update_command.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_builder.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/install_isolated_web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_location.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/encode/SkPngEncoder.h"

namespace web_app {
namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Field;
using ::testing::HasSubstr;
using ::testing::IsFalse;
using ::testing::IsTrue;
using ::testing::Lt;
using ::testing::Optional;
using ::testing::Property;

constexpr base::StringPiece kTestManifest = R"({
      "name": "$1",
      "version": "$2",
      "id": "/",
      "scope": "/",
      "start_url": "/",
      "display": "standalone",
      "icons": [
        {
          "src": "256x256-green.png",
          "sizes": "256x256",
          "type": "image/png"
        }
      ]
    })";

constexpr base::StringPiece kTestIconUrl = "/256x256-green.png";

std::string GetTestIconInString() {
  SkBitmap icon_bitmap = CreateSquareIcon(256, SK_ColorGREEN);
  SkDynamicMemoryWStream stream;
  EXPECT_THAT(SkPngEncoder::Encode(&stream, icon_bitmap.pixmap(), {}),
              IsTrue());
  sk_sp<SkData> icon_skdata = stream.detachAsData();
  return std::string(static_cast<const char*>(icon_skdata->data()),
                     icon_skdata->size());
}

// TODO(cmfcmf): Consider also adding tests for dev mode proxy.
class IsolatedWebAppUpdatePrepareAndStoreCommandBrowserTest
    : public IsolatedWebAppBrowserTestHarness,
      public ::testing::WithParamInterface<bool> {
 protected:
  using InstallResult = base::expected<InstallIsolatedWebAppCommandSuccess,
                                       InstallIsolatedWebAppCommandError>;
  using PrepareAndStoreUpdateResult =
      base::expected<void, IsolatedWebAppUpdatePrepareAndStoreCommandError>;

  using PendingUpdateInfo = WebApp::IsolationData::PendingUpdateInfo;

  void SetUp() override {
    ASSERT_THAT(scoped_temp_dir_.CreateUniqueTempDir(), IsTrue());

    installed_bundle_path_ = scoped_temp_dir_.GetPath().Append(
        base::FilePath::FromASCII("installed-bundle.swbn"));
    installed_location_ =
        is_dev_mode_ ? IsolatedWebAppLocation(
                           DevModeBundle{.path = installed_bundle_path_})
                     : IsolatedWebAppLocation(
                           InstalledBundle{.path = installed_bundle_path_});

    update_bundle_path_ = scoped_temp_dir_.GetPath().Append(
        base::FilePath::FromASCII("update-bundle.swbn"));
    update_location_ =
        is_dev_mode_
            ? IsolatedWebAppLocation(DevModeBundle{.path = update_bundle_path_})
            : IsolatedWebAppLocation(
                  InstalledBundle{.path = update_bundle_path_});

    IsolatedWebAppBrowserTestHarness::SetUp();
  }

  void CreateBundle(const base::Version& version,
                    const std::string& app_name,
                    const base::FilePath& path) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    TestSignedWebBundleBuilder builder(key_pair_);
    builder.AddManifest(base::ReplaceStringPlaceholders(
        kTestManifest, {app_name, version.GetString()}, nullptr));
    builder.AddPngImage(kTestIconUrl, GetTestIconInString());
    ASSERT_THAT(base::WriteFile(path, builder.Build().data), IsTrue());
  }

  void Install() {
    base::test::TestFuture<InstallResult> future;
    provider()->scheduler().InstallIsolatedWebApp(
        url_info_, installed_location_,
        /*expected_version=*/installed_version_,
        /*optional_keep_alive=*/nullptr,
        /*optional_profile_keep_alive=*/nullptr, future.GetCallback());
    InstallResult result = future.Take();
    ASSERT_THAT(result.has_value(), IsTrue());

    const WebApp* web_app =
        provider()->registrar_unsafe().GetAppById(url_info_.app_id());
    ASSERT_THAT(
        web_app,
        AllOf(Property("untranslated_name", &WebApp::untranslated_name,
                       Eq("installed app")),
              Property("isolation_data", &WebApp::isolation_data,
                       Optional(AllOf(
                           Field("location", &WebApp::IsolationData::location,
                                 Eq(installed_location_)),
                           Property("pending_update_info",
                                    &WebApp::IsolationData::pending_update_info,
                                    Eq(absl::nullopt)))))));
  }

  PrepareAndStoreUpdateResult PrepareAndStoreUpdateInfo(
      const PendingUpdateInfo& pending_update_info) {
    base::test::TestFuture<PrepareAndStoreUpdateResult> future;
    provider()->scheduler().PrepareAndStoreIsolatedWebAppUpdate(
        pending_update_info, url_info_,
        /*optional_keep_alive=*/nullptr,
        /*optional_profile_keep_alive=*/nullptr, future.GetCallback());
    return future.Take();
  }

  WebAppProvider* provider() {
    return WebAppProvider::GetForWebApps(profile());
  }

  bool is_dev_mode_ = GetParam();

  base::ScopedTempDir scoped_temp_dir_;

  web_package::WebBundleSigner::KeyPair key_pair_ =
      web_package::WebBundleSigner::KeyPair(kTestPublicKey, kTestPrivateKey);

  IsolatedWebAppUrlInfo url_info_ =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
          *web_package::SignedWebBundleId::Create(kTestEd25519WebBundleId));

  base::FilePath installed_bundle_path_;
  IsolatedWebAppLocation installed_location_;
  base::Version installed_version_ = base::Version("1.0.0");

  base::FilePath update_bundle_path_;
  IsolatedWebAppLocation update_location_;
  base::Version update_version_ = base::Version("2.0.0");
};

IN_PROC_BROWSER_TEST_P(IsolatedWebAppUpdatePrepareAndStoreCommandBrowserTest,
                       Succeeds) {
  ASSERT_NO_FATAL_FAILURE(CreateBundle(installed_version_, "installed app",
                                       installed_bundle_path_));
  ASSERT_NO_FATAL_FAILURE(
      CreateBundle(update_version_, "updated app", update_bundle_path_));

  ASSERT_NO_FATAL_FAILURE(Install());

  PrepareAndStoreUpdateResult result = PrepareAndStoreUpdateInfo(
      PendingUpdateInfo(update_location_, update_version_));
  EXPECT_THAT(result.has_value(), IsTrue()) << result.error();

  const WebApp* web_app =
      provider()->registrar_unsafe().GetAppById(url_info_.app_id());
  EXPECT_THAT(
      web_app,
      AllOf(Property("untranslated_name", &WebApp::untranslated_name,
                     Eq("installed app")),
            Property("isolation_data", &WebApp::isolation_data,
                     Optional(AllOf(
                         Field("location", &WebApp::IsolationData::location,
                               Eq(installed_location_)),
                         Field("version", &WebApp::IsolationData::version,
                               Eq(installed_version_)),
                         Property("pending_update_info",
                                  &WebApp::IsolationData::pending_update_info,
                                  Eq(PendingUpdateInfo(update_location_,
                                                       update_version_))))))));
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    IsolatedWebAppUpdatePrepareAndStoreCommandBrowserTest,
    ::testing::Bool(),
    [](::testing::TestParamInfo<bool> info) {
      return info.param ? "DevModeBundle" : "InstalledBundle";
    });

}  // namespace
}  // namespace web_app
