// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_prepare_and_store_update_command.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_builder.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/install_isolated_web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_location.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace web_app {
namespace {

using base::test::ValueIs;
using ::testing::_;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsTrue;
using ::testing::Lt;
using ::testing::Optional;

// TODO(cmfcmf): Consider also adding tests for dev mode proxy.
class IsolatedWebAppUpdatePrepareAndStoreCommandBrowserTest
    : public IsolatedWebAppBrowserTestHarness,
      public ::testing::WithParamInterface<bool> {
 protected:
  using InstallResult = base::expected<InstallIsolatedWebAppCommandSuccess,
                                       InstallIsolatedWebAppCommandError>;
  using PrepareAndStoreUpdateResult =
      IsolatedWebAppUpdatePrepareAndStoreCommandResult;

  using PendingUpdateInfo = WebApp::IsolationData::PendingUpdateInfo;

  void SetUp() override {
    ASSERT_THAT(scoped_temp_dir_.CreateUniqueTempDir(), IsTrue());

    installed_bundle_path_ = scoped_temp_dir_.GetPath().Append(
        base::FilePath::FromASCII("installed-bundle.swbn"));
    installed_location_input_ =
        is_dev_mode_ ? IsolatedWebAppLocation(
                           DevModeBundle{.path = installed_bundle_path_})
                     : IsolatedWebAppLocation(
                           InstalledBundle{.path = installed_bundle_path_});

    update_bundle_path_ = scoped_temp_dir_.GetPath().Append(
        base::FilePath::FromASCII("update-bundle.swbn"));
    update_location_input_ =
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
    TestSignedWebBundle bundle = TestSignedWebBundleBuilder::BuildDefault(
        TestSignedWebBundleBuilder::BuildOptions()
            .SetVersion(version)
            .SetKeyPair(key_pair_)
            .SetAppName(app_name));
    ASSERT_THAT(base::WriteFile(path, bundle.data), IsTrue());
  }

  InstallIsolatedWebAppCommandSuccess Install() {
    base::test::TestFuture<InstallResult> future;
    provider()->scheduler().InstallIsolatedWebApp(
        url_info_, installed_location_input_,
        /*expected_version=*/installed_version_,
        /*optional_keep_alive=*/nullptr,
        /*optional_profile_keep_alive=*/nullptr, future.GetCallback());
    InstallResult result = future.Take();
    EXPECT_THAT(result.has_value(), IsTrue());

    const WebApp* web_app =
        provider()->registrar_unsafe().GetAppById(url_info_.app_id());
    EXPECT_THAT(web_app,
                test::IwaIs(Eq("installed app"),
                            test::IsolationDataIs(
                                Eq(result->location), Eq(installed_version_),
                                /*controlled_frame_partitions=*/_,
                                /*pending_update_info=*/Eq(absl::nullopt))));
    return result.value();
  }

  PrepareAndStoreUpdateResult PrepareAndStoreUpdateInfo(
      const IsolatedWebAppUpdatePrepareAndStoreCommand::UpdateInfo&
          update_info) {
    base::test::TestFuture<PrepareAndStoreUpdateResult> future;
    provider()->scheduler().PrepareAndStoreIsolatedWebAppUpdate(
        update_info, url_info_,
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
  IsolatedWebAppLocation installed_location_input_;
  base::Version installed_version_ = base::Version("1.0.0");

  base::FilePath update_bundle_path_;
  IsolatedWebAppLocation update_location_input_;
  base::Version update_version_ = base::Version("2.0.0");
};

IN_PROC_BROWSER_TEST_P(IsolatedWebAppUpdatePrepareAndStoreCommandBrowserTest,
                       Succeeds) {
  ASSERT_NO_FATAL_FAILURE(CreateBundle(installed_version_, "installed app",
                                       installed_bundle_path_));
  ASSERT_NO_FATAL_FAILURE(
      CreateBundle(update_version_, "updated app", update_bundle_path_));

  IsolatedWebAppLocation final_install_location = Install().location;

  PrepareAndStoreUpdateResult result = PrepareAndStoreUpdateInfo(
      IsolatedWebAppUpdatePrepareAndStoreCommand::UpdateInfo(
          update_location_input_, update_version_));
  EXPECT_THAT(
      result,
      ValueIs(Field(
          "update_version",
          &IsolatedWebAppUpdatePrepareAndStoreCommandSuccess::update_version,
          Eq(update_version_))));

  const WebApp* web_app =
      provider()->registrar_unsafe().GetAppById(url_info_.app_id());
  EXPECT_THAT(
      web_app,
      test::IwaIs(
          Eq("installed app"),
          test::IsolationDataIs(
              Eq(final_install_location), Eq(installed_version_),
              /*controlled_frame_partitions=*/_,
              test::PendingUpdateInfoIs(result->location, update_version_))));
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
