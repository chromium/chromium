// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_prepare_and_store_update_command.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_builder.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_location.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_trust_checker.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_contents/web_app_url_loader.h"
#include "chrome/common/url_constants.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/browser/install_result_code.h"
#include "content/public/browser/web_contents.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "url/url_constants.h"

namespace web_app {
namespace {

using ::testing::AllOf;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::IsFalse;
using ::testing::IsNull;
using ::testing::IsTrue;
using ::testing::Matcher;
using ::testing::Property;
using ::testing::Return;

constexpr base::StringPiece kIconPath = "/icon.png";

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

Matcher<const WebApp*> HasNameAndIsolationData(
    base::StringPiece name,
    const absl::optional<WebApp::IsolationData>& isolation_data) {
  return AllOf(
      Property("untranslated_name", &WebApp::untranslated_name, Eq(name)),
      Property("isolation_data", &WebApp::isolation_data, Eq(isolation_data)));
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
    auto bundle =
        TestSignedWebBundleBuilder::BuildDefault({.version = update_version_});
    ASSERT_THAT(base::WriteFile(update_bundle_path_, bundle.data), IsTrue());
  }

  void InstallIwa() {
    WebAppProvider* provider = WebAppProvider::GetForWebApps(profile());

    std::unique_ptr<WebApp> isolated_web_app =
        test::CreateWebApp(url_info_.origin().GetURL());
    isolated_web_app->SetName("installed app");
    isolated_web_app->SetScope(isolated_web_app->start_url());
    isolated_web_app->SetIsolationData(
        WebApp::IsolationData(installed_location_, installed_version_));

    ScopedRegistryUpdate update = provider->sync_bridge_unsafe().BeginUpdate();
    update->CreateApp(std::move(isolated_web_app));
  }

  base::expected<void, IsolatedWebAppUpdatePrepareAndStoreCommandError>
  PrepareAndStoreUpdateInfo() {
    base::test::TestFuture<
        base::expected<void, IsolatedWebAppUpdatePrepareAndStoreCommandError>>
        future;
    provider()->scheduler().PrepareAndStoreIsolatedWebAppUpdate(
        pending_update_info(), url_info_, /*optional_keep_alive=*/nullptr,
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
                      kTestEd25519WebBundleId,
                      "/.well-known/_generated_install_page.html"}));
    auto& page_state = fake_web_contents_manager().GetOrCreatePageState(url);

    page_state.url_load_result = WebAppUrlLoaderResult::kUrlLoaded;
    page_state.error_code = webapps::InstallableStatusCode::NO_ERROR_DETECTED;
    page_state.manifest_url =
        url_info_.origin().GetURL().Resolve("manifest.webmanifest");
    page_state.valid_manifest_for_web_app = true;
    page_state.opt_manifest =
        CreateDefaultManifest(url_info_.origin().GetURL(), update_version_);

    return page_state;
  }

  WebApp::IsolationData::PendingUpdateInfo pending_update_info() {
    return WebApp::IsolationData::PendingUpdateInfo(
        InstalledBundle({.path = update_bundle_path_}), update_version_);
  }

  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  base::ScopedTempDir scoped_temp_dir_;

  web_package::WebBundleSigner::KeyPair key_pair_ =
      web_package::WebBundleSigner::KeyPair(kTestPublicKey, kTestPrivateKey);
  web_package::SignedWebBundleId web_bundle_id_ =
      *web_package::SignedWebBundleId::Create(kTestEd25519WebBundleId);
  IsolatedWebAppUrlInfo url_info_ =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id_);

  IsolatedWebAppLocation installed_location_ =
      InstalledBundle{.path = base::FilePath(FILE_PATH_LITERAL("a"))};
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

  auto result = PrepareAndStoreUpdateInfo();
  EXPECT_THAT(result.has_value(), IsTrue()) << result.error();

  const WebApp* web_app =
      provider()->registrar_unsafe().GetAppById(url_info_.app_id());
  EXPECT_THAT(web_app,
              HasNameAndIsolationData(
                  "installed app",
                  WebApp::IsolationData(installed_location_, installed_version_,
                                        /*controlled_frame_partitions=*/{},
                                        pending_update_info())));
}

TEST_F(IsolatedWebAppUpdatePrepareAndStoreCommandTest, FailsWhenShuttingDown) {
  provider()->Shutdown();

  auto result = PrepareAndStoreUpdateInfo();
  ASSERT_THAT(result.has_value(), IsFalse());
  EXPECT_THAT(result.error().message, HasSubstr("shutting down"));
}

TEST_F(IsolatedWebAppUpdatePrepareAndStoreCommandTest,
       FailsIfIwaIsNotInstalled) {
  WriteUpdateBundleToDisk();
  CreateDefaultPageState();

  auto result = PrepareAndStoreUpdateInfo();
  ASSERT_THAT(result.has_value(), IsFalse());
  EXPECT_THAT(result.error().message, HasSubstr("App is no longer installed"));

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

  auto result = PrepareAndStoreUpdateInfo();
  ASSERT_THAT(result.has_value(), IsFalse());
  EXPECT_THAT(result.error().message, HasSubstr("not an Isolated Web App"));

  const WebApp* web_app =
      provider()->registrar_unsafe().GetAppById(url_info_.app_id());
  EXPECT_THAT(web_app, HasNameAndIsolationData("installed app", absl::nullopt));
}

TEST_F(IsolatedWebAppUpdatePrepareAndStoreCommandTest,
       FailsIfInstalledAppIsOnHigherVersion) {
  installed_version_ = base::Version("3.0.0");
  InstallIwa();
  WriteUpdateBundleToDisk();
  CreateDefaultPageState();

  auto result = PrepareAndStoreUpdateInfo();
  ASSERT_THAT(result.has_value(), IsFalse());
  EXPECT_THAT(result.error().message,
              HasSubstr("Installed app is already on version"));

  const WebApp* web_app =
      provider()->registrar_unsafe().GetAppById(url_info_.app_id());
  EXPECT_THAT(web_app,
              HasNameAndIsolationData(
                  "installed app", WebApp::IsolationData(
                                       installed_location_, installed_version_,
                                       /*controlled_frame_partitions=*/{},
                                       /*pending_update_info=*/absl::nullopt)));
}

TEST_F(IsolatedWebAppUpdatePrepareAndStoreCommandTest,
       FailsIfInstalledAppHasOtherIwaLocationType) {
  installed_location_ = DevModeProxy{
      .proxy_url = url::Origin::Create(GURL("https://example.com"))};
  InstallIwa();
  WriteUpdateBundleToDisk();
  CreateDefaultPageState();

  auto result = PrepareAndStoreUpdateInfo();
  ASSERT_THAT(result.has_value(), IsFalse());
  EXPECT_THAT(
      result.error().message,
      HasSubstr(
          "Unable to update between different IsolatedWebAppLocation types"));

  const WebApp* web_app =
      provider()->registrar_unsafe().GetAppById(url_info_.app_id());
  EXPECT_THAT(web_app,
              HasNameAndIsolationData(
                  "installed app", WebApp::IsolationData(
                                       installed_location_, installed_version_,
                                       /*controlled_frame_partitions=*/{},
                                       /*pending_update_info=*/absl::nullopt)));
}

TEST_F(IsolatedWebAppUpdatePrepareAndStoreCommandTest, FailsIfAppNotTrusted) {
  InstallIwa();
  WriteUpdateBundleToDisk();
  CreateDefaultPageState();
  SetTrustedWebBundleIdsForTesting({});

  auto result = PrepareAndStoreUpdateInfo();
  ASSERT_THAT(result.has_value(), IsFalse());
  EXPECT_THAT(result.error().message,
              HasSubstr("The public key(s) are not trusted"));

  const WebApp* web_app =
      provider()->registrar_unsafe().GetAppById(url_info_.app_id());
  EXPECT_THAT(web_app,
              HasNameAndIsolationData(
                  "installed app", WebApp::IsolationData(
                                       installed_location_, installed_version_,
                                       /*controlled_frame_partitions=*/{},
                                       /*pending_update_info=*/absl::nullopt)));
}

TEST_F(IsolatedWebAppUpdatePrepareAndStoreCommandTest, FailsIfUrlLoadingFails) {
  InstallIwa();
  WriteUpdateBundleToDisk();
  auto& page_state = CreateDefaultPageState();
  page_state.url_load_result = WebAppUrlLoader::Result::kFailedErrorPageLoaded;

  auto result = PrepareAndStoreUpdateInfo();
  ASSERT_THAT(result.has_value(), IsFalse());
  EXPECT_THAT(result.error().message, HasSubstr("FailedErrorPageLoaded"));

  const WebApp* web_app =
      provider()->registrar_unsafe().GetAppById(url_info_.app_id());
  EXPECT_THAT(web_app,
              HasNameAndIsolationData(
                  "installed app", WebApp::IsolationData(
                                       installed_location_, installed_version_,
                                       /*controlled_frame_partitions=*/{},
                                       /*pending_update_info=*/absl::nullopt)));
}

TEST_F(IsolatedWebAppUpdatePrepareAndStoreCommandTest,
       FailsIfInstallabilityCheckFails) {
  InstallIwa();
  WriteUpdateBundleToDisk();
  CreateDefaultPageState();
  auto& page_state = CreateDefaultPageState();
  page_state.error_code =
      webapps::InstallableStatusCode::MANIFEST_MISSING_NAME_OR_SHORT_NAME;

  auto result = PrepareAndStoreUpdateInfo();
  ASSERT_THAT(result.has_value(), IsFalse());
  EXPECT_THAT(
      result.error().message,
      HasSubstr(" Manifest does not contain a 'name' or 'short_name' field"));

  const WebApp* web_app =
      provider()->registrar_unsafe().GetAppById(url_info_.app_id());
  EXPECT_THAT(web_app,
              HasNameAndIsolationData(
                  "installed app", WebApp::IsolationData(
                                       installed_location_, installed_version_,
                                       /*controlled_frame_partitions=*/{},
                                       /*pending_update_info=*/absl::nullopt)));
}

TEST_F(IsolatedWebAppUpdatePrepareAndStoreCommandTest,
       FailsIfManifestIsInvalid) {
  InstallIwa();
  WriteUpdateBundleToDisk();
  auto& page_state = CreateDefaultPageState();
  page_state.opt_manifest->scope = GURL("https://example.com/foo");

  auto result = PrepareAndStoreUpdateInfo();
  ASSERT_THAT(result.has_value(), IsFalse());
  EXPECT_THAT(result.error().message,
              HasSubstr("Scope should resolve to the origin"));

  const WebApp* web_app =
      provider()->registrar_unsafe().GetAppById(url_info_.app_id());
  EXPECT_THAT(web_app,
              HasNameAndIsolationData(
                  "installed app", WebApp::IsolationData(
                                       installed_location_, installed_version_,
                                       /*controlled_frame_partitions=*/{},
                                       /*pending_update_info=*/absl::nullopt)));
}

TEST_F(IsolatedWebAppUpdatePrepareAndStoreCommandTest,
       FailsIfIconDownloadFails) {
  InstallIwa();
  WriteUpdateBundleToDisk();
  CreateDefaultPageState();

  auto result = PrepareAndStoreUpdateInfo();
  ASSERT_THAT(result.has_value(), IsFalse());
  EXPECT_THAT(result.error().message,
              HasSubstr("Error during icon downloading"));

  const WebApp* web_app =
      provider()->registrar_unsafe().GetAppById(url_info_.app_id());
  EXPECT_THAT(web_app,
              HasNameAndIsolationData(
                  "installed app", WebApp::IsolationData(
                                       installed_location_, installed_version_,
                                       /*controlled_frame_partitions=*/{},
                                       /*pending_update_info=*/absl::nullopt)));
}

}  // namespace
}  // namespace web_app
