// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/commands/isolated_web_app_prepare_and_store_update_command.h"

#include <memory>
#include <string_view>

#include "base/containers/flat_set.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_trust_checker.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolation_data.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/web_contents/web_app_url_loader.h"
#include "components/webapps/isolated_web_apps/test_support/signing_keys.h"
#include "components/webapps/isolated_web_apps/types/source.h"
#include "components/webapps/isolated_web_apps/types/storage_location.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "url/url_constants.h"

namespace web_app {
namespace {

using base::test::ErrorIs;
using base::test::ValueIs;
using ::testing::_;
using ::testing::Eq;
using ::testing::Field;
using ::testing::HasSubstr;
using ::testing::IsFalse;
using ::testing::IsNull;
using ::testing::IsTrue;
using ::testing::Return;

constexpr std::string_view kIconPath = "/icon.png";

MATCHER_P(IsErrorWithMessage, message_matcher, "") {
  return ExplainMatchResult(
      ErrorIs(Field("message",
                    &IsolatedWebAppUpdatePrepareAndStoreCommandError::message,
                    message_matcher)),
      arg, result_listener);
}

class IsolatedWebAppUpdatePrepareAndStoreCommandTest : public WebAppTest {
 protected:
  void SetUp() override {
    WebAppTest::SetUp();

    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kIsolatedWebApps,
                              features::kIsolatedWebAppDevMode},
        /*disabled_features=*/{});
    SetTrustedWebBundleIdsForTesting({});

    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  std::unique_ptr<BundledIsolatedWebApp> WriteUpdateBundleToDisk(
      std::optional<IwaVersion> version = std::nullopt) {
    return IsolatedWebAppBuilder(
               ManifestBuilder().SetVersion(
                   version.value_or(update_version_).GetString()))
        .BuildBundle(test::GetDefaultEd25519KeyPair());
  }

  IsolatedWebAppUrlInfo InstallIwa() {
    auto bundle = IsolatedWebAppBuilder(ManifestBuilder().SetVersion(
                                            installed_version_.GetString()))
                      .BuildBundle(test::GetDefaultEd25519KeyPair());
    bundle->FakeInstallPageState(profile());
    bundle->TrustSigningKey();
    return bundle->Install(profile()).value();
  }

  base::expected<IsolatedWebAppUrlInfo, std::string> InstallProxyIwa() {
    auto web_bundle_id =
        web_package::SignedWebBundleId::CreateRandomForProxyMode();
    auto proxy_server =
        IsolatedWebAppBuilder(
            ManifestBuilder().SetVersion(installed_version_.GetString()))
            .BuildAndStartProxyServer();
    proxy_server->FakeInstallPageState(profile(), web_bundle_id);
    return proxy_server->Install(profile(), web_bundle_id);
  }

  IsolatedWebAppUpdatePrepareAndStoreCommandResult PrepareAndStoreUpdateInfo(
      const IsolatedWebAppUrlInfo& url_info,
      const BundledIsolatedWebApp& update_bundle,
      bool allow_downgrades = false) {
    base::test::TestFuture<IsolatedWebAppUpdatePrepareAndStoreCommandResult>
        future;
    provider()->scheduler().PrepareAndStoreIsolatedWebAppUpdate(
        IsolatedWebAppUpdatePrepareAndStoreCommand::UpdateInfo(
            IwaSourceBundleWithModeAndFileOp(
                update_bundle.path(),
                IwaSourceBundleModeAndFileOp::kProdModeMove),
            update_bundle.version(), allow_downgrades),
        url_info, /*optional_keep_alive=*/nullptr,
        /*optional_profile_keep_alive=*/nullptr, future.GetCallback());

    return future.Take();
  }

  WebAppProvider* provider() {
    return WebAppProvider::GetForWebApps(profile());
  }

  FakeWebContentsManager& fake_web_contents_manager() {
    return static_cast<FakeWebContentsManager&>(
        provider()->web_contents_manager());
  }

  base::flat_set<base::FilePath> GetIwaDirContent() {
    task_environment()->RunUntilIdle();

    base::flat_set<base::FilePath> existing_paths;
    const base::FilePath iwa_base_dir =
        profile()->GetPath().Append(kIwaDirName);

    if (!DirectoryExists(iwa_base_dir)) {
      return existing_paths;
    }

    base::FileEnumerator iwa_dir_content(
        iwa_base_dir, false, base::FileEnumerator::FileType::DIRECTORIES);
    for (auto path = iwa_dir_content.Next(); !path.empty();
         path = iwa_dir_content.Next()) {
      existing_paths.insert(path);
    }

    return existing_paths;
  }

  // Check that pending update was removed from the file structure.
  void CheckCleanup(const base::flat_set<base::FilePath>& installed_app_paths) {
    base::flat_set<base::FilePath> paths = GetIwaDirContent();
    EXPECT_EQ(paths, installed_app_paths);
  }

  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  IwaVersion installed_version_ = *IwaVersion::Create("1.0.0");
  IwaVersion update_version_ = *IwaVersion::Create("2.0.0");
  base::test::ScopedFeatureList scoped_feature_list_;

  IwaVersion downgrade_version_ = *IwaVersion::Create("0.7.0");
};

TEST_F(IsolatedWebAppUpdatePrepareAndStoreCommandTest, Succeeds) {
  auto url_info = InstallIwa();
  auto update_bundle = WriteUpdateBundleToDisk();
  update_bundle->FakeInstallPageState(profile());
  update_bundle->TrustSigningKey();

  ASSERT_OK_AND_ASSIGN(auto result,
                       PrepareAndStoreUpdateInfo(url_info, *update_bundle));
  EXPECT_THAT(
      result,
      Field("update_version",
            &IsolatedWebAppUpdatePrepareAndStoreCommandSuccess::update_version,
            Eq(update_version_)));
  IsolatedWebAppStorageLocation pending_location = result.location;

  const WebApp* web_app =
      provider()->registrar_unsafe().GetAppById(url_info.app_id());

  EXPECT_THAT(web_app,
              test::IwaIs(Eq("Test App"),
                          test::IsolationDataIs(
                              /*installed_location=*/_, Eq(installed_version_),
                              /*controlled_frame_partitions=*/_,
                              test::PendingUpdateInfoIs(
                                  Eq(pending_location), Eq(update_version_),
                                  /*integrity_block_data=*/_),
                              /*integrity_block_data=*/_)));
}

TEST_F(IsolatedWebAppUpdatePrepareAndStoreCommandTest,
       SucceedsWithLoweringTheVersionWhenDowngradesAreAllowed) {
  auto url_info = InstallIwa();
  auto downgrade_bundle = WriteUpdateBundleToDisk(downgrade_version_);
  downgrade_bundle->FakeInstallPageState(profile());
  downgrade_bundle->TrustSigningKey();

  ASSERT_OK_AND_ASSIGN(auto result,
                       PrepareAndStoreUpdateInfo(url_info, *downgrade_bundle,
                                                 /*allow_downgrades=*/true));
  EXPECT_THAT(
      result,
      Field("update_version",
            &IsolatedWebAppUpdatePrepareAndStoreCommandSuccess::update_version,
            Eq(downgrade_version_)));
  IsolatedWebAppStorageLocation pending_location = result.location;

  const WebApp* web_app =
      provider()->registrar_unsafe().GetAppById(url_info.app_id());
  EXPECT_THAT(web_app,
              test::IwaIs(Eq("Test App"),
                          test::IsolationDataIs(
                              /*installed_location=*/_, Eq(installed_version_),
                              /*controlled_frame_partitions=*/_,
                              test::PendingUpdateInfoIs(
                                  Eq(pending_location), Eq(downgrade_version_),
                                  /*integrity_block_data=*/_),
                              /*integrity_block_data=*/_)));
}

TEST_F(IsolatedWebAppUpdatePrepareAndStoreCommandTest,
       SucceedsIfVersionIsNotSpecified) {
  auto url_info = InstallIwa();
  auto update_bundle = WriteUpdateBundleToDisk();
  update_bundle->FakeInstallPageState(profile());
  update_bundle->TrustSigningKey();

  ASSERT_OK_AND_ASSIGN(auto result,
                       PrepareAndStoreUpdateInfo(url_info, *update_bundle));
  EXPECT_THAT(
      result,
      Field("update_version",
            &IsolatedWebAppUpdatePrepareAndStoreCommandSuccess::update_version,
            Eq(update_version_)));

  const WebApp* web_app =
      provider()->registrar_unsafe().GetAppById(url_info.app_id());

  EXPECT_THAT(web_app,
              test::IwaIs(Eq("Test App"),
                          test::IsolationDataIs(
                              /*installed_location=*/_, Eq(installed_version_),
                              /*controlled_frame_partitions=*/_,
                              test::PendingUpdateInfoIs(
                                  Eq(result.location), Eq(update_version_),
                                  /*integrity_block_data=*/_),
                              /*integrity_block_data=*/_)));
}

TEST_F(IsolatedWebAppUpdatePrepareAndStoreCommandTest, FailsWhenShuttingDown) {
  provider()->Shutdown();
  auto url_info = IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
      test::GetDefaultEd25519WebBundleId());
  auto update_bundle = WriteUpdateBundleToDisk();
  update_bundle->FakeInstallPageState(profile());
  update_bundle->TrustSigningKey();

  auto result = PrepareAndStoreUpdateInfo(url_info, *update_bundle);
  EXPECT_THAT(result, IsErrorWithMessage(HasSubstr("shutting down")));
}

TEST_F(IsolatedWebAppUpdatePrepareAndStoreCommandTest,
       FailsIfIwaIsNotInstalled) {
  auto url_info = IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
      test::GetDefaultEd25519WebBundleId());
  auto update_bundle = WriteUpdateBundleToDisk();
  update_bundle->FakeInstallPageState(profile());
  update_bundle->TrustSigningKey();

  auto result = PrepareAndStoreUpdateInfo(url_info, *update_bundle);
  EXPECT_THAT(result,
              IsErrorWithMessage(HasSubstr("App is no longer installed")));

  const WebApp* web_app =
      provider()->registrar_unsafe().GetAppById(url_info.app_id());
  EXPECT_THAT(web_app, IsNull());
}

TEST_F(IsolatedWebAppUpdatePrepareAndStoreCommandTest,
       FailsIfInstalledAppIsNotIsolated) {
  auto url_info = IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
      test::GetDefaultEd25519WebBundleId());
  test::InstallDummyWebApp(profile(), "installed app",
                           url_info.origin().GetURL());

  auto update_bundle = WriteUpdateBundleToDisk();
  update_bundle->FakeInstallPageState(profile());
  update_bundle->TrustSigningKey();

  auto result = PrepareAndStoreUpdateInfo(url_info, *update_bundle);
  EXPECT_THAT(result, IsErrorWithMessage(HasSubstr("not an Isolated Web App")));

  const WebApp* web_app =
      provider()->registrar_unsafe().GetAppById(url_info.app_id());
  EXPECT_THAT(web_app, test::IwaIs("installed app",
                                   /*isolation_data=*/Eq(std::nullopt)));
}

TEST_F(IsolatedWebAppUpdatePrepareAndStoreCommandTest,
       FailsIfInstalledAppIsOnHigherVersion) {
  installed_version_ = *IwaVersion::Create("3.0.0");
  EXPECT_THAT(update_version_, Eq(*IwaVersion::Create("2.0.0")));
  auto url_info = InstallIwa();
  auto update_bundle = WriteUpdateBundleToDisk();
  update_bundle->FakeInstallPageState(profile());
  update_bundle->TrustSigningKey();

  const base::flat_set<base::FilePath> existing_dirs = GetIwaDirContent();

  auto result = PrepareAndStoreUpdateInfo(url_info, *update_bundle);
  EXPECT_THAT(result, IsErrorWithMessage(HasSubstr(
                          "Version downgrades are not allowed. Installed app "
                          "version 3.0.0 is newer than the candidate "
                          "version 2.0.0.")));

  const WebApp* web_app =
      provider()->registrar_unsafe().GetAppById(url_info.app_id());
  EXPECT_THAT(web_app,
              test::IwaIs(Eq("Test App"),
                          test::IsolationDataIs(
                              /*installed_location=*/_, Eq(installed_version_),
                              /*controlled_frame_partitions=*/_,
                              /*pending_update_info=*/_,
                              /*integrity_block_data=*/_)));
  CheckCleanup(existing_dirs);
}

TEST_F(IsolatedWebAppUpdatePrepareAndStoreCommandTest,
       FailsIfInstalledAppIsOnHigherVersionAndNoExpectedVersionIsSpecified) {
  installed_version_ = *IwaVersion::Create("3.0.0");
  EXPECT_THAT(update_version_, Eq(*IwaVersion::Create("2.0.0")));
  auto url_info = InstallIwa();
  auto update_bundle = WriteUpdateBundleToDisk();
  update_bundle->FakeInstallPageState(profile());
  update_bundle->TrustSigningKey();

  const base::flat_set<base::FilePath> existing_dirs = GetIwaDirContent();

  auto result = PrepareAndStoreUpdateInfo(url_info, *update_bundle);
  EXPECT_THAT(result, IsErrorWithMessage(HasSubstr(
                          "Version downgrades are not allowed. Installed app "
                          "version 3.0.0 is newer than the candidate "
                          "version 2.0.0.")));

  const WebApp* web_app =
      provider()->registrar_unsafe().GetAppById(url_info.app_id());
  EXPECT_THAT(web_app,
              test::IwaIs(Eq("Test App"),
                          test::IsolationDataIs(
                              /*installed_location=*/_, Eq(installed_version_),
                              /*controlled_frame_partitions=*/_,
                              /*pending_update_info=*/_,
                              /*integrity_block_data=*/_)));
  CheckCleanup(existing_dirs);
}

TEST_F(IsolatedWebAppUpdatePrepareAndStoreCommandTest,
       FailsIfInstalledAppHasOtherIwaLocationType) {
  auto url_info = InstallProxyIwa().value();
  auto update_bundle = WriteUpdateBundleToDisk();
  update_bundle->FakeInstallPageState(profile());
  update_bundle->TrustSigningKey();

  const base::flat_set<base::FilePath> existing_dirs = GetIwaDirContent();

  auto result = PrepareAndStoreUpdateInfo(url_info, *update_bundle);
  EXPECT_THAT(result, IsErrorWithMessage(
                          HasSubstr("Unable to update between dev-mode and "
                                    "non-dev-mode storage location types")));

  const WebApp* web_app =
      provider()->registrar_unsafe().GetAppById(url_info.app_id());
  EXPECT_THAT(web_app,
              test::IwaIs(Eq("Test App"),
                          test::IsolationDataIs(
                              /*installed_location=*/_, Eq(installed_version_),
                              /*controlled_frame_partitions=*/_,
                              /*pending_update_info=*/_,
                              /*integrity_block_data=*/_)));
  CheckCleanup(existing_dirs);
}

TEST_F(IsolatedWebAppUpdatePrepareAndStoreCommandTest, FailsIfAppNotTrusted) {
  auto url_info = InstallIwa();
  auto update_bundle = WriteUpdateBundleToDisk();
  update_bundle->FakeInstallPageState(profile());

  const base::flat_set<base::FilePath> existing_dirs = GetIwaDirContent();

  SetTrustedWebBundleIdsForTesting({});

  auto result = PrepareAndStoreUpdateInfo(url_info, *update_bundle.get());
  EXPECT_THAT(result, IsErrorWithMessage(
                          HasSubstr("The public key(s) are not trusted")));

  const WebApp* web_app =
      provider()->registrar_unsafe().GetAppById(url_info.app_id());
  EXPECT_THAT(web_app,
              test::IwaIs(Eq("Test App"),
                          test::IsolationDataIs(
                              /*installed_location=*/_, Eq(installed_version_),
                              /*controlled_frame_partitions=*/_,
                              /*pending_update_info=*/_,
                              /*integrity_block_data=*/_)));
  CheckCleanup(existing_dirs);
}

TEST_F(IsolatedWebAppUpdatePrepareAndStoreCommandTest, FailsIfUrlLoadingFails) {
  auto url_info = InstallIwa();
  auto update_bundle = WriteUpdateBundleToDisk();
  auto& page_state = update_bundle->FakeInstallPageState(profile());
  page_state.url_load_result =
      webapps::WebAppUrlLoaderResult::kFailedErrorPageLoaded;

  const base::flat_set<base::FilePath> existing_dirs = GetIwaDirContent();

  auto result = PrepareAndStoreUpdateInfo(url_info, *update_bundle);
  EXPECT_THAT(result, IsErrorWithMessage(HasSubstr("FailedErrorPageLoaded")));

  const WebApp* web_app =
      provider()->registrar_unsafe().GetAppById(url_info.app_id());
  EXPECT_THAT(web_app,
              test::IwaIs(Eq("Test App"),
                          test::IsolationDataIs(
                              /*installed_location=*/_, Eq(installed_version_),
                              /*controlled_frame_partitions=*/_,
                              /*pending_update_info=*/_,
                              /*integrity_block_data=*/_)));
  CheckCleanup(existing_dirs);
}

TEST_F(IsolatedWebAppUpdatePrepareAndStoreCommandTest,
       FailsIfInstallabilityCheckFails) {
  auto url_info = InstallIwa();
  auto update_bundle = WriteUpdateBundleToDisk();
  auto& page_state = update_bundle->FakeInstallPageState(profile());
  page_state.error_code =
      webapps::InstallableStatusCode::MANIFEST_MISSING_NAME_OR_SHORT_NAME;

  const base::flat_set<base::FilePath> existing_dirs = GetIwaDirContent();

  auto result = PrepareAndStoreUpdateInfo(url_info, *update_bundle);
  EXPECT_THAT(result,
              IsErrorWithMessage(HasSubstr(
                  "Manifest does not contain a 'name' or 'short_name' field")));

  const WebApp* web_app =
      provider()->registrar_unsafe().GetAppById(url_info.app_id());
  EXPECT_THAT(web_app,
              test::IwaIs(Eq("Test App"),
                          test::IsolationDataIs(
                              /*installed_location=*/_, Eq(installed_version_),
                              /*controlled_frame_partitions=*/_,
                              /*pending_update_info=*/_,
                              /*integrity_block_data=*/_)));
  CheckCleanup(existing_dirs);
}

TEST_F(IsolatedWebAppUpdatePrepareAndStoreCommandTest,
       FailsIfManifestIsInvalid) {
  auto url_info = InstallIwa();
  auto update_bundle = WriteUpdateBundleToDisk();
  auto& page_state = update_bundle->FakeInstallPageState(profile());
  page_state.manifest_before_default_processing->scope =
      GURL("https://example.com/foo/");

  const base::flat_set<base::FilePath> existing_dirs = GetIwaDirContent();

  auto result = PrepareAndStoreUpdateInfo(url_info, *update_bundle.get());
  EXPECT_THAT(result, IsErrorWithMessage(
                          HasSubstr("Scope should resolve to the origin")));

  const WebApp* web_app =
      provider()->registrar_unsafe().GetAppById(url_info.app_id());
  EXPECT_THAT(web_app,
              test::IwaIs(Eq("Test App"),
                          test::IsolationDataIs(
                              /*installed_location=*/_, Eq(installed_version_),
                              /*controlled_frame_partitions=*/_,
                              /*pending_update_info=*/_,
                              /*integrity_block_data=*/_)));
  CheckCleanup(existing_dirs);
}

TEST_F(IsolatedWebAppUpdatePrepareAndStoreCommandTest,
       FailsIfIconDownloadFails) {
  auto url_info = InstallIwa();
  auto update_bundle = WriteUpdateBundleToDisk();
  update_bundle->FakeInstallPageState(profile());
  fake_web_contents_manager().DeleteIconState(
      url_info.origin().GetURL().Resolve(kIconPath));

  const base::flat_set<base::FilePath> existing_dirs = GetIwaDirContent();

  auto result = PrepareAndStoreUpdateInfo(url_info, *update_bundle);
  EXPECT_THAT(result,
              IsErrorWithMessage(HasSubstr("Error during icon downloading")));

  const WebApp* web_app =
      provider()->registrar_unsafe().GetAppById(url_info.app_id());
  EXPECT_THAT(web_app,
              test::IwaIs(Eq("Test App"),
                          test::IsolationDataIs(
                              /*installed_location=*/_, Eq(installed_version_),
                              /*controlled_frame_partitions=*/_,
                              /*pending_update_info=*/_,
                              /*integrity_block_data=*/_)));
  CheckCleanup(existing_dirs);
}

}  // namespace
}  // namespace web_app
