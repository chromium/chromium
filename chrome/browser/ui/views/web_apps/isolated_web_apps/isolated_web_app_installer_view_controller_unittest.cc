// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_installer_view_controller.h"

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/version.h"
#include "chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_installer_model.h"
#include "chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_installer_view.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_location.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_metadata.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "url/gurl.h"

namespace web_app {
namespace {

using ::testing::_;
using ::testing::Invoke;

constexpr base::StringPiece kIconPath = "/icon.png";

IsolatedWebAppUrlInfo CreateAndWriteTestBundle(
    const base::FilePath& bundle_path,
    const std::string& version) {
  TestSignedWebBundleBuilder::BuildOptions bundle_options =
      TestSignedWebBundleBuilder::BuildOptions().SetVersion(
          base::Version(version));
  auto bundle = TestSignedWebBundleBuilder::BuildDefault(bundle_options);
  base::ScopedAllowBlockingForTesting allow_blocking;
  CHECK(base::WriteFile(bundle_path, bundle.data));
  return IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(bundle.id);
}

blink::mojom::ManifestPtr CreateDefaultManifest(const GURL& iwa_url,
                                                const base::Version version) {
  auto manifest = blink::mojom::Manifest::New();
  manifest->id = iwa_url;
  manifest->scope = iwa_url.Resolve("/");
  manifest->start_url = iwa_url.Resolve("/testing-start-url.html");
  manifest->display = DisplayMode::kStandalone;
  manifest->short_name = u"test app name";
  manifest->version = base::UTF8ToUTF16(version.GetString());

  blink::Manifest::ImageResource icon;
  icon.src = iwa_url.Resolve(kIconPath);
  icon.purpose = {blink::mojom::ManifestImageResource_Purpose::ANY};
  icon.type = u"image/png";
  icon.sizes = {gfx::Size(256, 256)};
  manifest->icons.push_back(icon);

  return manifest;
}

class MockView : public IsolatedWebAppInstallerView,
                 public IsolatedWebAppInstallerView::Delegate {
 public:
  MockView() : IsolatedWebAppInstallerView(this) {}

  MOCK_METHOD(void, ShowDisabledScreen, (), (override));
  MOCK_METHOD(void, ShowGetMetadataScreen, (), (override));
  MOCK_METHOD(void,
              UpdateGetMetadataProgress,
              (double percent, int minutes_remaining),
              (override));
  MOCK_METHOD(void,
              ShowMetadataScreen,
              (const SignedWebBundleMetadata& bundle_metadata),
              (override));
  MOCK_METHOD(void,
              ShowInstallScreen,
              (const SignedWebBundleMetadata& bundle_metadata),
              (override));
  MOCK_METHOD(void,
              UpdateInstallProgress,
              (double percent, int minutes_remaining),
              (override));
  MOCK_METHOD(void,
              ShowInstallSuccessScreen,
              (const SignedWebBundleMetadata& bundle_metadata),
              (override));

  // `IsolatedWebAppInstallerView::Delegate`:
  MOCK_METHOD(void, OnSettingsLinkClicked, (), (override));
};

class IsolatedWebAppInstallerViewControllerTest : public ::testing::Test {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {features::kIsolatedWebApps, features::kIsolatedWebAppDevMode}, {});
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());

    TestingProfile::Builder profile_builder;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    profile_builder.SetIsMainProfile(true);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
    profile_ = profile_builder.Build();

    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  TestingProfile* profile() { return profile_.get(); }

  FakeWebAppProvider* fake_provider() {
    return FakeWebAppProvider::Get(profile());
  }

  base::FilePath CreateBundlePath(const std::string& bundle_filename) {
    return scoped_temp_dir_.GetPath().Append(
        base::FilePath::FromASCII(bundle_filename));
  }

  void MockIconAndPageState(const IsolatedWebAppUrlInfo& url_info) {
    GURL iwa_url = url_info.origin().GetURL();
    auto& fake_web_contents_manager = static_cast<FakeWebContentsManager&>(
        fake_provider()->web_contents_manager());
    auto& icon_state = fake_web_contents_manager.GetOrCreateIconState(
        iwa_url.Resolve(kIconPath));
    icon_state.bitmaps = {CreateSquareIcon(32, SK_ColorWHITE)};

    GURL url(
        base::StrCat({chrome::kIsolatedAppScheme, url::kStandardSchemeSeparator,
                      kTestEd25519WebBundleId,
                      "/.well-known/_generated_install_page.html"}));
    auto& page_state = fake_web_contents_manager.GetOrCreatePageState(url);

    page_state.url_load_result = WebAppUrlLoaderResult::kUrlLoaded;
    page_state.error_code = webapps::InstallableStatusCode::NO_ERROR_DETECTED;
    page_state.manifest_url = iwa_url.Resolve("manifest.webmanifest");
    page_state.valid_manifest_for_web_app = true;
    page_state.opt_manifest =
        CreateDefaultManifest(iwa_url, base::Version("7.7.7"));
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::ScopedTempDir scoped_temp_dir_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(IsolatedWebAppInstallerViewControllerTest,
       ValidBundleTransitionsToShowMetadataScreen) {
  base::FilePath bundle_path = CreateBundlePath("test_bundle.swbn");
  IsolatedWebAppUrlInfo url_info = CreateAndWriteTestBundle(bundle_path, "1.0");
  MockIconAndPageState(url_info);

  IsolatedWebAppInstallerModel model(bundle_path);
  IsolatedWebAppInstallerViewController controller(profile(), fake_provider(),
                                                   &model);
  testing::StrictMock<MockView> view;
  controller.SetViewForTesting(&view);

  base::test::TestFuture<SignedWebBundleMetadata> bundle_metadata;
  EXPECT_CALL(view, ShowGetMetadataScreen());
  EXPECT_CALL(view, ShowMetadataScreen(_))
      .WillOnce(
          Invoke(&bundle_metadata,
                 &base::test::TestFuture<SignedWebBundleMetadata>::SetValue));

  controller.Start();

  EXPECT_TRUE(bundle_metadata.Wait());
}

}  // namespace
}  // namespace web_app
