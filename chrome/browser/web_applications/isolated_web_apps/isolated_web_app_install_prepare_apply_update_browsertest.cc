// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/component_updater/iwa_key_distribution_component_installer.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/install_isolated_web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_apply_update_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_install_source.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_prepare_and_store_update_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_trust_checker.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/integrity_block_data_matcher.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/key_distribution/test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/test_signed_web_bundle_builder.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/test_support/signed_web_bundles/web_bundle_signer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {
namespace {

using base::test::ErrorIs;
using base::test::HasValue;
using base::test::ValueIs;
using ::testing::_;
using ::testing::Eq;
using ::testing::Field;
using ::testing::HasSubstr;
using ::testing::IsTrue;

class IsolatedWebAppInstallPrepareApplyUpdateCommandBrowserTest
    : public IsolatedWebAppBrowserTestHarness,
      public ::testing::WithParamInterface<bool> {
 protected:
  using InstallResult = base::expected<InstallIsolatedWebAppCommandSuccess,
                                       InstallIsolatedWebAppCommandError>;
  using PrepareAndStoreUpdateResult =
      IsolatedWebAppUpdatePrepareAndStoreCommandResult;
  using ApplyUpdateResult =
      base::expected<void, IsolatedWebAppApplyUpdateCommandError>;

  IsolatedWebAppInstallSource GetInstallSource(
      const base::FilePath& bundle_path) const {
    return is_dev_mode_
               ? IsolatedWebAppInstallSource::FromDevUi(
                     IwaSourceBundleDevModeWithFileOp(bundle_path,
                                                      kDefaultBundleDevFileOp))
               : IsolatedWebAppInstallSource::FromGraphicalInstaller(
                     IwaSourceBundleProdModeWithFileOp(
                         bundle_path, IwaSourceBundleProdFileOp::kCopy));
  }

  IwaSourceBundleWithModeAndFileOp GetUpdateSource(
      const base::FilePath& bundle_path) const {
    return IwaSourceBundleWithModeAndFileOp(
        bundle_path, is_dev_mode_
                         ? IwaSourceBundleModeAndFileOp::kDevModeMove
                         : IwaSourceBundleModeAndFileOp::kProdModeMove);
  }

  InstallResult Install(const web_package::SignedWebBundleId& web_bundle_id,
                        const base::FilePath& bundle_path,
                        const std::optional<base::Version>& expected_version) {
    base::test::TestFuture<InstallResult> future;
    provider()->scheduler().InstallIsolatedWebApp(
        IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id),
        GetInstallSource(bundle_path), expected_version,
        /*optional_keep_alive=*/nullptr,
        /*optional_profile_keep_alive=*/nullptr, future.GetCallback());
    return future.Take();
  }

  PrepareAndStoreUpdateResult PrepareAndStoreUpdateInfo(
      const web_package::SignedWebBundleId& web_bundle_id,
      const base::FilePath& update_bundle_path,
      const base::Version& update_version) {
    base::test::TestFuture<PrepareAndStoreUpdateResult> future;
    provider()->scheduler().PrepareAndStoreIsolatedWebAppUpdate(
        IsolatedWebAppUpdatePrepareAndStoreCommand::UpdateInfo(
            GetUpdateSource(update_bundle_path), update_version),
        IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id),
        /*optional_keep_alive=*/nullptr,
        /*optional_profile_keep_alive=*/nullptr, future.GetCallback());
    return future.Take();
  }

  ApplyUpdateResult ApplyUpdate(
      const web_package::SignedWebBundleId& web_bundle_id) {
    base::test::TestFuture<ApplyUpdateResult> future;
    provider()->scheduler().ApplyPendingIsolatedWebAppUpdate(
        IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id),
        /*optional_keep_alive=*/nullptr,
        /*optional_profile_keep_alive=*/nullptr, future.GetCallback());
    return future.Take();
  }

  WebAppProvider* provider() {
    return WebAppProvider::GetForWebApps(profile());
  }

  const WebApp* GetIsolatedWebAppFor(
      const web_package::SignedWebBundleId& web_bundle_id) {
    return provider()->registrar_unsafe().GetAppById(
        IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id)
            .app_id());
  }

  bool is_dev_mode_ = GetParam();

  base::test::ScopedFeatureList features_{
      component_updater::kIwaKeyDistributionComponent};
};

IN_PROC_BROWSER_TEST_P(
    IsolatedWebAppInstallPrepareApplyUpdateCommandBrowserTest,
    Succeeds) {
  auto web_bundle_id = test::GetDefaultEd25519WebBundleId();
  SetTrustedWebBundleIdsForTesting({web_bundle_id});

  auto iwa =
      IsolatedWebAppBuilder(
          ManifestBuilder().SetName("installed app").SetVersion("1.0.0"))
          .BuildBundle(web_bundle_id, {test::GetDefaultEd25519KeyPair()});

  auto update_iwa =
      IsolatedWebAppBuilder(
          ManifestBuilder().SetName("updated app").SetVersion("2.0.0"))
          .BuildBundle(web_bundle_id, {test::GetDefaultEd25519KeyPair(),
                                       test::GetDefaultEcdsaP256KeyPair()});

  // Step 1: Install `iwa` and validate web app data.
  ASSERT_OK_AND_ASSIGN(auto install_result,
                       Install(web_bundle_id, iwa->path(), iwa->version()));

  ASSERT_THAT(
      GetIsolatedWebAppFor(web_bundle_id),
      test::IwaIs(Eq("installed app"),
                  test::IsolationDataIs(
                      install_result.location, iwa->version(),
                      /*controlled_frame_partitions=*/_,
                      /*pending_update_info=*/std::nullopt,
                      /*integrity_block_data=*/
                      test::IntegrityBlockDataPublicKeysAre(
                          test::GetDefaultEd25519KeyPair().public_key))));

  // Step 2: Prepare the update based on `update_iwa` and validate pending info.
  ASSERT_OK_AND_ASSIGN(
      auto prep_store_update_result,
      PrepareAndStoreUpdateInfo(web_bundle_id, update_iwa->path(),
                                update_iwa->version()));
  ASSERT_THAT(
      prep_store_update_result,
      Field(&IsolatedWebAppUpdatePrepareAndStoreCommandSuccess::update_version,
            Eq(update_iwa->version())));

  ASSERT_THAT(
      GetIsolatedWebAppFor(web_bundle_id),
      test::IwaIs(
          Eq("installed app"),
          test::IsolationDataIs(
              install_result.location, iwa->version(),
              /*controlled_frame_partitions=*/_,
              /*pending_update_info=*/
              test::PendingUpdateInfoIs(
                  prep_store_update_result.location, update_iwa->version(),
                  test::IntegrityBlockDataPublicKeysAre(
                      test::GetDefaultEd25519KeyPair().public_key,
                      test::GetDefaultEcdsaP256KeyPair().public_key)),
              /*integrity_block_data=*/
              test::IntegrityBlockDataPublicKeysAre(
                  test::GetDefaultEd25519KeyPair().public_key))));

  // Step 3: Apply the update and ensure that pending info has been successfully
  // transferred.
  ASSERT_THAT(ApplyUpdate(web_bundle_id), HasValue());

  ASSERT_THAT(
      GetIsolatedWebAppFor(web_bundle_id),
      test::IwaIs(Eq("updated app"),
                  test::IsolationDataIs(
                      prep_store_update_result.location, update_iwa->version(),
                      /*controlled_frame_partitions=*/_,
                      /*pending_update_info=*/std::nullopt,
                      /*integrity_block_data=*/
                      test::IntegrityBlockDataPublicKeysAre(
                          test::GetDefaultEd25519KeyPair().public_key,
                          test::GetDefaultEcdsaP256KeyPair().public_key))));
}

IN_PROC_BROWSER_TEST_P(
    IsolatedWebAppInstallPrepareApplyUpdateCommandBrowserTest,
    SucceedsSameVersionWithKeyRotation) {
  auto web_bundle_id = test::GetDefaultEd25519WebBundleId();
  SetTrustedWebBundleIdsForTesting({web_bundle_id});

  base::Version version("1.0.0");

  // IWA signed by a Ed25519 key.
  auto iwa =
      IsolatedWebAppBuilder(ManifestBuilder()
                                .SetName("installed app")
                                .SetVersion(version.GetString()))
          .BuildBundle(web_bundle_id, {test::GetDefaultEd25519KeyPair()});

  // Same-version IWA signed by a Ecdsa P-256 key.
  auto update_iwa =
      IsolatedWebAppBuilder(ManifestBuilder()
                                .SetName("updated app")
                                .SetVersion(version.GetString()))
          .BuildBundle(web_bundle_id, {test::GetDefaultEcdsaP256KeyPair()});

  auto ed25519_pk = test::GetDefaultEd25519KeyPair().public_key;
  auto ecdsa_p256_pk = test::GetDefaultEcdsaP256KeyPair().public_key;

  // Step 1: Install `iwa` and validate web app data.
  ASSERT_OK_AND_ASSIGN(auto install_result,
                       Install(web_bundle_id, iwa->path(), version));

  ASSERT_THAT(
      GetIsolatedWebAppFor(web_bundle_id),
      test::IwaIs(Eq("installed app"),
                  test::IsolationDataIs(
                      install_result.location, version,
                      /*controlled_frame_partitions=*/_,
                      /*pending_update_info=*/std::nullopt,
                      /*integrity_block_data=*/
                      test::IntegrityBlockDataPublicKeysAre(ed25519_pk))));

  // Step 2: Ensure that update fails without key rotation.
  ASSERT_THAT(
      PrepareAndStoreUpdateInfo(web_bundle_id, update_iwa->path(), version),
      ErrorIs(_));

  // Step 3: point `web_bundle_id` (which is Ed25519-based) to the default
  // Ecdsa P-256 public key via the Key Distribution Component. This should
  // allow us to perform a same-version update to replace the underlying
  // bundle signed by a corrupted key.
  EXPECT_THAT(
      test::InstallIwaKeyDistributionComponent(
          base::Version("0.1.0"), web_bundle_id.id(), ecdsa_p256_pk.bytes()),
      HasValue());

  // Step 4: Prepare the update based on `update_iwa` and validate pending info.
  ASSERT_OK_AND_ASSIGN(
      auto prep_store_update_result,
      PrepareAndStoreUpdateInfo(web_bundle_id, update_iwa->path(), version));
  ASSERT_THAT(
      prep_store_update_result,
      Field(&IsolatedWebAppUpdatePrepareAndStoreCommandSuccess::update_version,
            Eq(version)));

  ASSERT_THAT(
      GetIsolatedWebAppFor(web_bundle_id),
      test::IwaIs(Eq("installed app"),
                  test::IsolationDataIs(
                      install_result.location, version,
                      /*controlled_frame_partitions=*/_,
                      /*pending_update_info=*/
                      test::PendingUpdateInfoIs(
                          prep_store_update_result.location, version,
                          test::IntegrityBlockDataPublicKeysAre(ecdsa_p256_pk)),
                      /*integrity_block_data=*/
                      test::IntegrityBlockDataPublicKeysAre(ed25519_pk))));

  // Step 5: Apply the update and ensure that pending info has been
  // successfully transferred.
  ASSERT_THAT(ApplyUpdate(web_bundle_id), HasValue());

  ASSERT_THAT(
      GetIsolatedWebAppFor(web_bundle_id),
      test::IwaIs(Eq("updated app"),
                  test::IsolationDataIs(
                      prep_store_update_result.location, version,
                      /*controlled_frame_partitions=*/_,
                      /*pending_update_info=*/std::nullopt,
                      /*integrity_block_data=*/
                      test::IntegrityBlockDataPublicKeysAre(ecdsa_p256_pk))));
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    IsolatedWebAppInstallPrepareApplyUpdateCommandBrowserTest,
    ::testing::Bool(),
    [](::testing::TestParamInfo<bool> info) {
      return info.param ? "DevModeBundle" : "InstalledBundle";
    });

}  // namespace
}  // namespace web_app
