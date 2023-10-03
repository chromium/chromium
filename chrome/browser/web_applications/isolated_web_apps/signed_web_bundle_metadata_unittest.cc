// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_metadata.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_location.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_trust_checker.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_contents/web_app_url_loader.h"
#include "chrome/common/url_constants.h"
#include "components/web_package/test_support/signed_web_bundles/web_bundle_signer.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace web_app {
namespace {

using testing::HasSubstr;
using testing::Property;

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

class SignedWebBundleMetadataTest : public WebAppTest {
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
          TestSignedWebBundleBuilder::BuildOptions()) {
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
                                                    base::Version("3.4.5"));
  }

 private:
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  base::ScopedTempDir scoped_temp_dir_;
  base::FilePath bundle_path_;
};

TEST_F(SignedWebBundleMetadataTest, Succeeds) {
  IsolatedWebAppUrlInfo url_info = WriteBundleToDisk();
  SetTrustedWebBundleIdsForTesting({url_info.web_bundle_id()});
  IsolatedWebAppLocation location = InstalledBundle{.path = bundle_path()};
  FakeWebContentsManager& fake_web_contents_manager =
      static_cast<FakeWebContentsManager&>(
          fake_provider().web_contents_manager());
  MockIconAndPageState(fake_web_contents_manager, url_info);

  base::test::TestFuture<base::expected<SignedWebBundleMetadata, std::string>>
      metadata_future;
  SignedWebBundleMetadata::Create(profile(), &fake_provider(), url_info,
                                  location, metadata_future.GetCallback());
  base::expected<SignedWebBundleMetadata, std::string> metadata =
      metadata_future.Get();

  EXPECT_TRUE(metadata.has_value());
  EXPECT_THAT(metadata.value(),
              Property(&SignedWebBundleMetadata::app_name, u"test app name"));
  EXPECT_THAT(metadata.value(), Property(&SignedWebBundleMetadata::version,
                                         base::Version("3.4.5")));
  EXPECT_EQ(metadata.value().app_id(), url_info.app_id());
}

TEST_F(SignedWebBundleMetadataTest, FailsWhenWebBundleIdNotTrusted) {
  IsolatedWebAppUrlInfo url_info = WriteBundleToDisk();
  SetTrustedWebBundleIdsForTesting({});
  IsolatedWebAppLocation location = InstalledBundle{.path = bundle_path()};
  FakeWebContentsManager& fake_web_contents_manager =
      static_cast<FakeWebContentsManager&>(
          fake_provider().web_contents_manager());
  MockIconAndPageState(fake_web_contents_manager, url_info);

  base::test::TestFuture<base::expected<SignedWebBundleMetadata, std::string>>
      metadata_future;
  SignedWebBundleMetadata::Create(profile(), &fake_provider(), url_info,
                                  location, metadata_future.GetCallback());
  base::expected<SignedWebBundleMetadata, std::string> metadata =
      metadata_future.Get();

  EXPECT_FALSE(metadata.has_value());
  EXPECT_THAT(metadata.error(), HasSubstr("not trusted"));
}

TEST_F(SignedWebBundleMetadataTest, FailsWhenBundleInvalid) {
  IsolatedWebAppUrlInfo url_info = WriteBundleToDisk(
      TestSignedWebBundleBuilder::BuildOptions().SetErrorsForTesting(
          {web_package::WebBundleSigner::ErrorForTesting::
               kInvalidIntegrityBlockStructure}));
  SetTrustedWebBundleIdsForTesting({url_info.web_bundle_id()});
  IsolatedWebAppLocation location = InstalledBundle{.path = bundle_path()};
  FakeWebContentsManager& fake_web_contents_manager =
      static_cast<FakeWebContentsManager&>(
          fake_provider().web_contents_manager());
  MockIconAndPageState(fake_web_contents_manager, url_info);

  base::test::TestFuture<base::expected<SignedWebBundleMetadata, std::string>>
      metadata_future;
  SignedWebBundleMetadata::Create(profile(), &fake_provider(), url_info,
                                  location, metadata_future.GetCallback());
  base::expected<SignedWebBundleMetadata, std::string> metadata =
      metadata_future.Get();

  EXPECT_FALSE(metadata.has_value());
  EXPECT_THAT(metadata.error(), HasSubstr("Wrong array size or magic bytes"));
}

TEST_F(SignedWebBundleMetadataTest, FailsWhenLocationIsDevModeProxy) {
  IsolatedWebAppUrlInfo url_info =
      WriteBundleToDisk(TestSignedWebBundleBuilder::BuildOptions());
  SetTrustedWebBundleIdsForTesting({url_info.web_bundle_id()});
  IsolatedWebAppLocation location = DevModeProxy{
      .proxy_url = url::Origin::Create(GURL("http://example.com"))};
  FakeWebContentsManager& fake_web_contents_manager =
      static_cast<FakeWebContentsManager&>(
          fake_provider().web_contents_manager());
  MockIconAndPageState(fake_web_contents_manager, url_info);

  base::test::TestFuture<base::expected<SignedWebBundleMetadata, std::string>>
      metadata_future;
  SignedWebBundleMetadata::Create(profile(), &fake_provider(), url_info,
                                  location, metadata_future.GetCallback());
  base::expected<SignedWebBundleMetadata, std::string> metadata =
      metadata_future.Get();

  EXPECT_FALSE(metadata.has_value());
  EXPECT_THAT(metadata.error(),
              HasSubstr("No Signed Web Bundle Metadata for dev-mode proxy"));
}

}  // namespace
}  // namespace web_app
