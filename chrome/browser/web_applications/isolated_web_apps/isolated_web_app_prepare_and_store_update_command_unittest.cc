// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_prepare_and_store_update_command.h"

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
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_source.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_storage_location.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_trust_checker.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolation_data.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/test_signed_web_bundle_builder.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/url_constants.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/web_contents/web_app_url_loader.h"
#include "content/public/browser/web_contents.h"
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

blink::mojom::ManifestPtr CreateDefaultManifest(const GURL& application_url,
                                                const base::Version version) {
  auto manifest = blink::mojom::Manifest::New();
  manifest->id = application_url.DeprecatedGetOriginAsURL();
  manifest->scope = application_url.Resolve("/");
  manifest->start_url = application_url.Resolve("/testing-start-url.html");
  manifest->display = DisplayMode::kStandalone;
  manifest->short_name = u"updated app";
  manifest->version = base::UTF8ToUTF16(version.GetString());

  blink::Manifest::ImageResource icon;
  icon.src = application_url.Resolve(kIconPath);
  icon.purpose = {blink::mojom::ManifestImageResource_Purpose::ANY};
  icon.type = u"image/png";
  icon.sizes = {gfx::Size(256, 256)};
  manifest->icons.push_back(icon);

  return manifest;
}

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
    ASSERT_THAT(scoped_temp_dir_.CreateUniqueTempDir(), IsTrue());
    update_bundle_path_ = scoped_temp_dir_.GetPath().Append(
        base::FilePath::FromASCII("update-bundle.swbn"));

    SetTrustedWebBundleIdsForTesting({web_bundle_id_});

    WebAppTest::SetUp();

    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  void WriteUpdateBundleToDisk() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    auto bundle = TestSignedWebBundleBuilder::BuildDefault(
        TestSignedWebBundleBuilder::BuildOptions().SetVersion(update_version_));
    ASSERT_THAT(base::WriteFile(update_bundle_path_, bundle.data), IsTrue());
  }

  void InstallIwa() {
    WebAppProvider* provider = WebAppProvider::GetForWebApps(profile());

    std::unique_ptr<WebApp> isolated_web_app =
        test::CreateWebApp(url_info_.origin().GetURL());
    isolated_web_app->SetName("installed app");
    isolated_web_app->SetScope(isolated_web_app->start_url());
    isolated_web_app->SetIsolationData(
        IsolationData::Builder(installed_location_, installed_version_)
            .Build());

    ScopedRegistryUpdate update = provider->sync_bridge_unsafe().BeginUpdate();
    update->CreateApp(std::move(isolated_web_app));
  }

  IsolatedWebAppUpdatePrepareAndStoreCommandResult PrepareAndStoreUpdateInfo(
      const std::optional<base::Version>& expected_version) {
    base::test::TestFuture<IsolatedWebAppUpdatePrepareAndStoreCommandResult>
        future;
    provider()->scheduler().PrepareAndStoreIsolatedWebAppUpdate(
        IsolatedWebAppUpdatePrepareAndStoreCommand::UpdateInfo(
            IwaSourceBundleWithModeAndFileOp(
                update_bundle_path_,
                IwaSourceBundleModeAndFileOp::kProdModeMove),
            expected_version),
        url_info_, /*optional_keep_alive=*/nullptr,
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

  FakeWebContentsManager::FakePageState& CreateDefaultPageState() {
    GURL url(
        base::StrCat({chrome::kIsolatedAppScheme, url::kStandardSchemeSeparator,
                      test::GetDefaultEd25519WebBundleId().id(),
                      "/.well-known/_generated_install_page.html"}));
    auto& page_state = fake_web_contents_manager().GetOrCreatePageState(url);

    page_state.url_load_result = webapps::WebAppUrlLoaderResult::kUrlLoaded;
    page_state.error_code = webapps::InstallableStatusCode::NO_ERROR_DETECTED;
    page_state.manifest_url =
        url_info_.origin().GetURL().Resolve("manifest.webmanifest");
    page_state.valid_manifest_for_web_app = true;
    page_state.manifest_before_default_processing =
        CreateDefaultManifest(url_info_.origin().GetURL(), update_version_);

    return page_state;
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
  base::ScopedTempDir scoped_temp_dir_;

  web_package::test::Ed25519KeyPair key_pair_ =
      test::GetDefaultEd25519KeyPair();
  web_package::SignedWebBundleId web_bundle_id_ =
      test::GetDefaultEd25519WebBundleId();
  IsolatedWebAppUrlInfo url_info_ =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id_);

  IsolatedWebAppStorageLocation installed_location_ =
      IwaStorageOwnedBundle{"a", /*dev_mode=*/false};
  base::Version installed_version_ = base::Version("1.0.0");

  base::FilePath update_bundle_path_;
  base::Version update_version_ = base::Version("2.0.0");
};

TEST_F(IsolatedWebAppUpdatePrepareAndStoreCommandTest, Succeeds) {
  InstallIwa();
  WriteUpdateBundleToDisk();
  CreateDefaultPageState();

  auto& icon_state = fake_web_contents_manager().GetOrCreateIconState(
      url_info_.origin().GetURL().Resolve(kIconPath));
  icon_state.bitmaps = {web_app::CreateSquareIcon(32, SK_ColorWHITE)};

  ASSERT_OK_AND_ASSIGN(auto result, PrepareAndStoreUpdateInfo(update_version_));
  EXPECT_THAT(
      result,
      Field("update_version",
            &IsolatedWebAppUpdatePrepareAndStoreCommandSuccess::update_version,
            Eq(update_version_)));
  IsolatedWebAppStorageLocation pending_location = result.location;

  const WebApp* web_app =
      provider()->registrar_unsafe().GetAppById(url_info_.app_id());
  EXPECT_THAT(web_app,
              test::IwaIs(Eq("installed app"),
                          test::IsolationDataIs(
                              Eq(installed_location_), Eq(installed_version_),
                              /*controlled_frame_partitions=*/_,
                              test::PendingUpdateInfoIs(
                                  Eq(pending_location), Eq(update_version_),
                                  /*integrity_block_data=*/_),
                              /*integrity_block_data=*/_)));
}

TEST_F(IsolatedWebAppUpdatePrepareAndStoreCommandTest,
       SucceedsIfVersionIsNotSpecified) {
  InstallIwa();
  WriteUpdateBundleToDisk();
  CreateDefaultPageState();

  auto& icon_state = fake_web_contents_manager().GetOrCreateIconState(
      url_info_.origin().GetURL().Resolve(kIconPath));
  icon_state.bitmaps = {web_app::CreateSquareIcon(32, SK_ColorWHITE)};

  ASSERT_OK_AND_ASSIGN(auto result, PrepareAndStoreUpdateInfo(update_version_));
  EXPECT_THAT(
      result,
      Field("update_version",
            &IsolatedWebAppUpdatePrepareAndStoreCommandSuccess::update_version,
            Eq(update_version_)));

  const WebApp* web_app =
      provider()->registrar_unsafe().GetAppById(url_info_.app_id());

  EXPECT_THAT(web_app,
              test::IwaIs(Eq("installed app"),
                          test::IsolationDataIs(
                              Eq(installed_location_), Eq(installed_version_),
                              /*controlled_frame_partitions=*/_,
                              test::PendingUpdateInfoIs(
                                  Eq(result.location), Eq(update_version_),
                                  /*integrity_block_data=*/_),
                              /*integrity_block_data=*/_)));
}

TEST_F(IsolatedWebAppUpdatePrepareAndStoreCommandTest, FailsWhenShuttingDown) {
  provider()->Shutdown();

  auto result = PrepareAndStoreUpdateInfo(update_version_);
  EXPECT_THAT(result, IsErrorWithMessage(HasSubstr("shutting down")));
}

TEST_F(IsolatedWebAppUpdatePrepareAndStoreCommandTest,
       FailsIfIwaIsNotInstalled) {
  WriteUpdateBundleToDisk();
  CreateDefaultPageState();

  auto result = PrepareAndStoreUpdateInfo(update_version_);
  EXPECT_THAT(result,
              IsErrorWithMessage(HasSubstr("App is no longer installed")));

  const WebApp* web_app =
      provider()->registrar_unsafe().GetAppById(url_info_.app_id());
  EXPECT_THAT(web_app, IsNull());
}

TEST_F(IsolatedWebAppUpdatePrepareAndStoreCommandTest,
       FailsIfInstalledAppIsNotIsolated) {
  test::InstallDummyWebApp(profile(), "installed app",
                           url_info_.origin().GetURL());

  WriteUpdateBundleToDisk();
  CreateDefaultPageState();

  auto result = PrepareAndStoreUpdateInfo(update_version_);
  EXPECT_THAT(result, IsErrorWithMessage(HasSubstr("not an Isolated Web App")));

  const WebApp* web_app =
      provider()->registrar_unsafe().GetAppById(url_info_.app_id());
  EXPECT_THAT(web_app, test::IwaIs("installed app",
                                   /*isolation_data=*/Eq(std::nullopt)));
}

TEST_F(IsolatedWebAppUpdatePrepareAndStoreCommandTest,
       FailsIfInstalledAppIsOnHigherVersion) {
  installed_version_ = base::Version("3.0.0");
  EXPECT_THAT(update_version_, Eq(base::Version("2.0.0")));
  InstallIwa();
  const base::flat_set<base::FilePath> existing_dirs = GetIwaDirContent();

  WriteUpdateBundleToDisk();
  CreateDefaultPageState();

  auto result = PrepareAndStoreUpdateInfo(update_version_);
  EXPECT_THAT(result, IsErrorWithMessage(
                          HasSubstr("Installed app is already on version")));

  const WebApp* web_app =
      provider()->registrar_unsafe().GetAppById(url_info_.app_id());
  EXPECT_THAT(web_app, test::IwaIs(Eq("installed app"),
                                   IsolationData::Builder(installed_location_,
                                                          installed_version_)
                                       .Build()));
  CheckCleanup(existing_dirs);
}

TEST_F(IsolatedWebAppUpdatePrepareAndStoreCommandTest,
       FailsIfInstalledAppIsOnHigherVersionAndNoExpectedVersionIsSpecified) {
  installed_version_ = base::Version("3.0.0");
  EXPECT_THAT(update_version_, Eq(base::Version("2.0.0")));
  InstallIwa();
  const base::flat_set<base::FilePath> existing_dirs = GetIwaDirContent();

  WriteUpdateBundleToDisk();
  CreateDefaultPageState();

  auto result = PrepareAndStoreUpdateInfo(/*expected_version=*/std::nullopt);
  EXPECT_THAT(result, IsErrorWithMessage(
                          HasSubstr("Installed app is already on version")));

  const WebApp* web_app =
      provider()->registrar_unsafe().GetAppById(url_info_.app_id());
  EXPECT_THAT(web_app,
              test::IwaIs(Eq("installed app"),
                          Eq(IsolationData::Builder(installed_location_,
                                                    installed_version_)
                                 .Build())));
  CheckCleanup(existing_dirs);
}

TEST_F(IsolatedWebAppUpdatePrepareAndStoreCommandTest,
       FailsIfInstalledAppHasOtherIwaLocationType) {
  installed_location_ =
      IwaStorageProxy{url::Origin::Create(GURL("https://example.com"))};
  InstallIwa();
  const base::flat_set<base::FilePath> existing_dirs = GetIwaDirContent();

  WriteUpdateBundleToDisk();
  CreateDefaultPageState();

  auto result = PrepareAndStoreUpdateInfo(update_version_);
  EXPECT_THAT(result, IsErrorWithMessage(
                          HasSubstr("Unable to update between dev-mode and "
                                    "non-dev-mode storage location types")));

  const WebApp* web_app =
      provider()->registrar_unsafe().GetAppById(url_info_.app_id());
  EXPECT_THAT(web_app,
              test::IwaIs(Eq("installed app"),
                          Eq(IsolationData::Builder(installed_location_,
                                                    installed_version_)
                                 .Build())));

  CheckCleanup(existing_dirs);
}

TEST_F(IsolatedWebAppUpdatePrepareAndStoreCommandTest, FailsIfAppNotTrusted) {
  InstallIwa();
  const base::flat_set<base::FilePath> existing_dirs = GetIwaDirContent();

  WriteUpdateBundleToDisk();
  CreateDefaultPageState();
  SetTrustedWebBundleIdsForTesting({});

  auto result = PrepareAndStoreUpdateInfo(update_version_);
  EXPECT_THAT(result, IsErrorWithMessage(
                          HasSubstr("The public key(s) are not trusted")));

  const WebApp* web_app =
      provider()->registrar_unsafe().GetAppById(url_info_.app_id());
  EXPECT_THAT(web_app,
              test::IwaIs(Eq("installed app"),
                          Eq(IsolationData::Builder(installed_location_,
                                                    installed_version_)
                                 .Build())));

  CheckCleanup(existing_dirs);
}

TEST_F(IsolatedWebAppUpdatePrepareAndStoreCommandTest, FailsIfUrlLoadingFails) {
  InstallIwa();

  const base::flat_set<base::FilePath> existing_dirs = GetIwaDirContent();
  WriteUpdateBundleToDisk();
  auto& page_state = CreateDefaultPageState();
  page_state.url_load_result =
      webapps::WebAppUrlLoaderResult::kFailedErrorPageLoaded;

  auto result = PrepareAndStoreUpdateInfo(update_version_);
  EXPECT_THAT(result, IsErrorWithMessage(HasSubstr("FailedErrorPageLoaded")));

  const WebApp* web_app =
      provider()->registrar_unsafe().GetAppById(url_info_.app_id());
  EXPECT_THAT(web_app,
              test::IwaIs(Eq("installed app"),
                          Eq(IsolationData::Builder(installed_location_,
                                                    installed_version_)
                                 .Build())));

  CheckCleanup(existing_dirs);
}

TEST_F(IsolatedWebAppUpdatePrepareAndStoreCommandTest,
       FailsIfInstallabilityCheckFails) {
  InstallIwa();
  const base::flat_set<base::FilePath> existing_dirs = GetIwaDirContent();

  WriteUpdateBundleToDisk();
  CreateDefaultPageState();
  auto& page_state = CreateDefaultPageState();
  page_state.error_code =
      webapps::InstallableStatusCode::MANIFEST_MISSING_NAME_OR_SHORT_NAME;

  auto result = PrepareAndStoreUpdateInfo(update_version_);
  EXPECT_THAT(result,
              IsErrorWithMessage(HasSubstr(
                  "Manifest does not contain a 'name' or 'short_name' field")));

  const WebApp* web_app =
      provider()->registrar_unsafe().GetAppById(url_info_.app_id());
  EXPECT_THAT(web_app,
              test::IwaIs(Eq("installed app"),
                          Eq(IsolationData::Builder(installed_location_,
                                                    installed_version_)
                                 .Build())));

  CheckCleanup(existing_dirs);
}

TEST_F(IsolatedWebAppUpdatePrepareAndStoreCommandTest,
       FailsIfManifestIsInvalid) {
  InstallIwa();
  const base::flat_set<base::FilePath> existing_dirs = GetIwaDirContent();

  WriteUpdateBundleToDisk();
  auto& page_state = CreateDefaultPageState();
  page_state.manifest_before_default_processing->scope =
      GURL("https://example.com/foo/");

  auto result = PrepareAndStoreUpdateInfo(update_version_);
  EXPECT_THAT(result, IsErrorWithMessage(
                          HasSubstr("Scope should resolve to the origin")));

  const WebApp* web_app =
      provider()->registrar_unsafe().GetAppById(url_info_.app_id());
  EXPECT_THAT(web_app,
              test::IwaIs(Eq("installed app"),
                          Eq(IsolationData::Builder(installed_location_,
                                                    installed_version_)
                                 .Build())));

  CheckCleanup(existing_dirs);
}

TEST_F(IsolatedWebAppUpdatePrepareAndStoreCommandTest,
       FailsIfIconDownloadFails) {
  InstallIwa();
  const base::flat_set<base::FilePath> existing_dirs = GetIwaDirContent();

  WriteUpdateBundleToDisk();
  CreateDefaultPageState();

  auto result = PrepareAndStoreUpdateInfo(update_version_);
  EXPECT_THAT(result,
              IsErrorWithMessage(HasSubstr("Error during icon downloading")));

  const WebApp* web_app =
      provider()->registrar_unsafe().GetAppById(url_info_.app_id());
  EXPECT_THAT(web_app,
              test::IwaIs(Eq("installed app"),
                          Eq(IsolationData::Builder(installed_location_,
                                                    installed_version_)
                                 .Build())));

  CheckCleanup(existing_dirs);
}

}  // namespace
}  // namespace web_app
