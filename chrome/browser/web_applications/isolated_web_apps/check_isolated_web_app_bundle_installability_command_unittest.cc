// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/check_isolated_web_app_bundle_installability_command.h"

#include <memory>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "base/types/expected.h"
#include "base/version.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_location.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_trust_checker.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_metadata.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_contents/web_app_url_loader.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "content/public/common/content_features.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace web_app {
namespace {

using testing::HasSubstr;
using InstallabilityCheckResult =
    CheckIsolatedWebAppBundleInstallabilityCommand::InstallabilityCheckResult;

constexpr base::StringPiece kIconPath = "/icon.png";

blink::mojom::ManifestPtr CreateDefaultManifest(const GURL& application_url,
                                                const base::Version version) {
  auto manifest = blink::mojom::Manifest::New();
  manifest->id = application_url.DeprecatedGetOriginAsURL();
  manifest->scope = application_url.Resolve("/");
  manifest->start_url = application_url.Resolve("/testing-start-url.html");
  manifest->display = DisplayMode::kStandalone;
  manifest->short_name = u"test app name";
  manifest->version = base::UTF8ToUTF16(version.GetString());

  blink::Manifest::ImageResource icon;
  icon.src = application_url.Resolve(kIconPath);
  icon.purpose = {blink::mojom::ManifestImageResource_Purpose::ANY};
  icon.type = u"image/png";
  icon.sizes = {gfx::Size(256, 256)};
  manifest->icons.push_back(icon);

  return manifest;
}

class CheckIsolatedWebAppBundleInstallabilityCommandTest : public WebAppTest {
 protected:
  void SetUp() override {
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    bundle_path_ = scoped_temp_dir_.GetPath().Append(
        base::FilePath::FromASCII("test_bundle.swbn"));
    WebAppTest::SetUp();
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  IsolatedWebAppUrlInfo WriteBundleToDisk(
      TestSignedWebBundleBuilder::BuildOptions options =
          TestSignedWebBundleBuilder::BuildOptions().SetVersion(
              base::Version("7.7.7"))) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    auto bundle = TestSignedWebBundleBuilder::BuildDefault(options);
    CHECK(base::WriteFile(bundle_path_, bundle.data));

    return IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(bundle.id);
  }

  const base::FilePath& bundle_path() const { return bundle_path_; }

  void MockIconAndPageState(FakeWebContentsManager& fake_web_contents_manager,
                            IsolatedWebAppUrlInfo url_info) {
    auto& icon_state = fake_web_contents_manager.GetOrCreateIconState(
        url_info.origin().GetURL().Resolve(kIconPath));
    icon_state.bitmaps = {CreateSquareIcon(32, SK_ColorWHITE)};

    GURL url(
        base::StrCat({chrome::kIsolatedAppScheme, url::kStandardSchemeSeparator,
                      kTestEd25519WebBundleId,
                      "/.well-known/_generated_install_page.html"}));
    auto& page_state = fake_web_contents_manager.GetOrCreatePageState(url);

    page_state.url_load_result = WebAppUrlLoaderResult::kUrlLoaded;
    page_state.error_code = webapps::InstallableStatusCode::NO_ERROR_DETECTED;
    page_state.manifest_url =
        url_info.origin().GetURL().Resolve("manifest.webmanifest");
    page_state.valid_manifest_for_web_app = true;
    page_state.opt_manifest = CreateDefaultManifest(url_info.origin().GetURL(),
                                                    base::Version("7.7.7"));
  }

  void ScheduleCommand(
      const SignedWebBundleMetadata& bundle_metadata,
      base::OnceCallback<void(InstallabilityCheckResult,
                              absl::optional<base::Version>)> callback) {
    fake_provider().scheduler().CheckIsolatedWebAppBundleInstallability(
        bundle_metadata, std::move(callback));
  }

  FakeWebContentsManager& fake_web_contents_manager() {
    return static_cast<FakeWebContentsManager&>(
        fake_provider().web_contents_manager());
  }

  SignedWebBundleMetadata GetBundleMetadata(
      const IsolatedWebAppUrlInfo& url_info,
      const IsolatedWebAppLocation& location) {
    SetTrustedWebBundleIdsForTesting({url_info.web_bundle_id()});
    MockIconAndPageState(fake_web_contents_manager(), url_info);

    base::test::TestFuture<base::expected<SignedWebBundleMetadata, std::string>>
        metadata_future;
    SignedWebBundleMetadata::Create(profile(), &fake_provider(), url_info,
                                    location, metadata_future.GetCallback());
    base::expected<SignedWebBundleMetadata, std::string> metadata =
        metadata_future.Get();
    CHECK(metadata.has_value());
    return metadata.value();
  }

  void AddMockIwaToRegistrar(const IsolatedWebAppUrlInfo& url_info,
                             const base::Version& version,
                             const IsolatedWebAppLocation& location) {
    std::unique_ptr<WebApp> isolated_web_app =
        test::CreateWebApp(url_info.origin().GetURL());
    isolated_web_app->SetName("installed app");
    isolated_web_app->SetScope(isolated_web_app->start_url());
    isolated_web_app->SetIsolationData(
        WebApp::IsolationData(location, version));

    ScopedRegistryUpdate update =
        fake_provider().sync_bridge_unsafe().BeginUpdate();
    update->CreateApp(std::move(isolated_web_app));
  }

 private:
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  base::ScopedTempDir scoped_temp_dir_;
  base::FilePath bundle_path_;
};

TEST_F(CheckIsolatedWebAppBundleInstallabilityCommandTest,
       SucceedsWhenAppNotInRegistrar) {
  IsolatedWebAppUrlInfo url_info = WriteBundleToDisk();
  IsolatedWebAppLocation location = InstalledBundle{.path = bundle_path()};
  SignedWebBundleMetadata metadata = GetBundleMetadata(url_info, location);

  base::test::TestFuture<InstallabilityCheckResult,
                         absl::optional<base::Version>>
      command_future;
  ScheduleCommand(metadata, command_future.GetCallback());
  InstallabilityCheckResult result = command_future.Get<0>();
  absl::optional<base::Version> installed_version = command_future.Get<1>();

  EXPECT_EQ(result, InstallabilityCheckResult::kInstallable);
  EXPECT_FALSE(installed_version.has_value());
}

TEST_F(CheckIsolatedWebAppBundleInstallabilityCommandTest,
       SucceedsWhenInstalledAppLowerVersion) {
  IsolatedWebAppUrlInfo url_info = WriteBundleToDisk();
  IsolatedWebAppLocation location = InstalledBundle{.path = bundle_path()};
  AddMockIwaToRegistrar(url_info, base::Version("7.7.6"), location);
  SignedWebBundleMetadata metadata = GetBundleMetadata(url_info, location);

  base::test::TestFuture<InstallabilityCheckResult,
                         absl::optional<base::Version>>
      command_future;
  ScheduleCommand(metadata, command_future.GetCallback());
  InstallabilityCheckResult result = command_future.Get<0>();
  absl::optional<base::Version> installed_version = command_future.Get<1>();

  EXPECT_EQ(result, InstallabilityCheckResult::kUpdatable);
  EXPECT_EQ(installed_version, base::Version("7.7.6"));
}

TEST_F(CheckIsolatedWebAppBundleInstallabilityCommandTest,
       FailsWhenInstalledAppSameVersion) {
  IsolatedWebAppUrlInfo url_info = WriteBundleToDisk();
  IsolatedWebAppLocation location = InstalledBundle{.path = bundle_path()};
  AddMockIwaToRegistrar(url_info, base::Version("7.7.7"), location);
  SignedWebBundleMetadata metadata = GetBundleMetadata(url_info, location);

  base::test::TestFuture<InstallabilityCheckResult,
                         absl::optional<base::Version>>
      command_future;
  ScheduleCommand(metadata, command_future.GetCallback());
  InstallabilityCheckResult result = command_future.Get<0>();
  absl::optional<base::Version> installed_version = command_future.Get<1>();

  EXPECT_EQ(result, InstallabilityCheckResult::kOutdated);
  EXPECT_EQ(installed_version, base::Version("7.7.7"));
}

TEST_F(CheckIsolatedWebAppBundleInstallabilityCommandTest,
       FailsWhenInstalledAppHigherVersion) {
  IsolatedWebAppUrlInfo url_info = WriteBundleToDisk();
  IsolatedWebAppLocation location = InstalledBundle{.path = bundle_path()};
  AddMockIwaToRegistrar(url_info, base::Version("7.7.8"), location);
  SignedWebBundleMetadata metadata = GetBundleMetadata(url_info, location);

  base::test::TestFuture<InstallabilityCheckResult,
                         absl::optional<base::Version>>
      command_future;
  ScheduleCommand(metadata, command_future.GetCallback());
  InstallabilityCheckResult result = command_future.Get<0>();
  absl::optional<base::Version> installed_version = command_future.Get<1>();

  EXPECT_EQ(result, InstallabilityCheckResult::kOutdated);
  EXPECT_EQ(installed_version, base::Version("7.7.8"));
}

class CheckIsolatedWebAppBundleInstallabilityCommandDevModeTest
    : public CheckIsolatedWebAppBundleInstallabilityCommandTest {
 protected:
  void SetUp() override {
    scoped_feature_list.InitWithFeatures(
        {features::kIsolatedWebApps, features::kIsolatedWebAppDevMode}, {});
    CheckIsolatedWebAppBundleInstallabilityCommandTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list;
};

TEST_F(CheckIsolatedWebAppBundleInstallabilityCommandDevModeTest,
       SucceedsWhenInstalledAppLowerVersion) {
  IsolatedWebAppUrlInfo url_info = WriteBundleToDisk();
  IsolatedWebAppLocation location = InstalledBundle{.path = bundle_path()};
  AddMockIwaToRegistrar(url_info, base::Version("7.7.6"), location);
  SignedWebBundleMetadata metadata = GetBundleMetadata(url_info, location);

  base::test::TestFuture<InstallabilityCheckResult,
                         absl::optional<base::Version>>
      command_future;
  ScheduleCommand(metadata, command_future.GetCallback());
  InstallabilityCheckResult result = command_future.Get<0>();
  absl::optional<base::Version> installed_version = command_future.Get<1>();

  EXPECT_EQ(result, InstallabilityCheckResult::kUpdatable);
  EXPECT_EQ(installed_version, base::Version("7.7.6"));
}

TEST_F(CheckIsolatedWebAppBundleInstallabilityCommandDevModeTest,
       SucceedsWhenInstalledAppSameVersion) {
  IsolatedWebAppUrlInfo url_info = WriteBundleToDisk();
  IsolatedWebAppLocation location = InstalledBundle{.path = bundle_path()};
  AddMockIwaToRegistrar(url_info, base::Version("7.7.7"), location);
  SignedWebBundleMetadata metadata = GetBundleMetadata(url_info, location);

  base::test::TestFuture<InstallabilityCheckResult,
                         absl::optional<base::Version>>
      command_future;
  ScheduleCommand(metadata, command_future.GetCallback());
  InstallabilityCheckResult result = command_future.Get<0>();
  absl::optional<base::Version> installed_version = command_future.Get<1>();

  EXPECT_EQ(result, InstallabilityCheckResult::kUpdatable);
  EXPECT_EQ(installed_version, base::Version("7.7.7"));
}

TEST_F(CheckIsolatedWebAppBundleInstallabilityCommandDevModeTest,
       FailsWhenInstalledAppHigherVersion) {
  IsolatedWebAppUrlInfo url_info = WriteBundleToDisk();
  IsolatedWebAppLocation location = InstalledBundle{.path = bundle_path()};
  AddMockIwaToRegistrar(url_info, base::Version("7.7.8"), location);
  SignedWebBundleMetadata metadata = GetBundleMetadata(url_info, location);

  base::test::TestFuture<InstallabilityCheckResult,
                         absl::optional<base::Version>>
      command_future;
  ScheduleCommand(metadata, command_future.GetCallback());
  InstallabilityCheckResult result = command_future.Get<0>();
  absl::optional<base::Version> installed_version = command_future.Get<1>();

  EXPECT_EQ(result, InstallabilityCheckResult::kOutdated);
  EXPECT_EQ(installed_version, base::Version("7.7.8"));
}

}  // namespace
}  // namespace web_app
