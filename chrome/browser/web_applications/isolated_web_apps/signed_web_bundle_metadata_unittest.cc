// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_metadata.h"

#include <string_view>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_source.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_storage_location.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_trust_checker.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/test_signed_web_bundle_builder.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/common/url_constants.h"
#include "components/web_package/test_support/signed_web_bundles/web_bundle_signer.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/web_contents/web_app_url_loader.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace web_app {
namespace {

using base::test::ErrorIs;
using base::test::ValueIs;
using testing::AllOf;
using testing::Eq;
using testing::HasSubstr;
using testing::Property;

constexpr std::string_view kIconPath = "/icon.png";

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
    WebAppTest::SetUp();
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  IsolatedWebAppUrlInfo WriteBundleToDisk(
      TestSignedWebBundleBuilder::BuildOptions options =
          TestSignedWebBundleBuilder::BuildOptions()) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    auto bundle = TestSignedWebBundleBuilder::BuildDefault(options);
    base::FilePath bundle_path = location_.GetPath(profile()->GetPath());
    EXPECT_TRUE(base::CreateDirectory(bundle_path.DirName()));
    EXPECT_TRUE(base::WriteFile(bundle_path, bundle.data));

    return IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(bundle.id);
  }

  IwaSourceBundleProdMode bundle_source() const {
    return IwaSourceBundleProdMode(location_.GetPath(profile()->GetPath()));
  }

  void MockIconAndPageState(FakeWebContentsManager& fake_web_contents_manager,
                            IsolatedWebAppUrlInfo url_info) {
    auto& icon_state = fake_web_contents_manager.GetOrCreateIconState(
        url_info.origin().GetURL().Resolve(kIconPath));
    icon_state.bitmaps = {CreateSquareIcon(32, SK_ColorWHITE)};

    GURL url(
        base::StrCat({chrome::kIsolatedAppScheme, url::kStandardSchemeSeparator,
                      test::GetDefaultEd25519WebBundleId().id(),
                      "/.well-known/_generated_install_page.html"}));
    auto& page_state = fake_web_contents_manager.GetOrCreatePageState(url);

    page_state.url_load_result = webapps::WebAppUrlLoaderResult::kUrlLoaded;
    page_state.error_code = webapps::InstallableStatusCode::NO_ERROR_DETECTED;
    page_state.manifest_url =
        url_info.origin().GetURL().Resolve("manifest.webmanifest");
    page_state.valid_manifest_for_web_app = true;
    page_state.manifest_before_default_processing = CreateDefaultManifest(
        url_info.origin().GetURL(), base::Version("3.4.5"));
  }

 private:
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  IwaStorageOwnedBundle location_{"some_folder", /*dev_mode=*/false};
};

TEST_F(SignedWebBundleMetadataTest, Succeeds) {
  IsolatedWebAppUrlInfo url_info = WriteBundleToDisk();
  SetTrustedWebBundleIdsForTesting({url_info.web_bundle_id()});
  FakeWebContentsManager& fake_web_contents_manager =
      static_cast<FakeWebContentsManager&>(
          fake_provider().web_contents_manager());
  MockIconAndPageState(fake_web_contents_manager, url_info);

  base::test::TestFuture<base::expected<SignedWebBundleMetadata, std::string>>
      metadata_future;
  SignedWebBundleMetadata::Create(profile(), &fake_provider(), url_info,
                                  bundle_source(),
                                  metadata_future.GetCallback());
  base::expected<SignedWebBundleMetadata, std::string> metadata =
      metadata_future.Get();
  EXPECT_THAT(
      metadata,
      ValueIs(AllOf(
          Property(&SignedWebBundleMetadata::app_name, Eq(u"test app name")),
          Property(&SignedWebBundleMetadata::version,
                   Eq(base::Version("3.4.5"))),
          Property(&SignedWebBundleMetadata::app_id, Eq(url_info.app_id())))));
}

TEST_F(SignedWebBundleMetadataTest, FailsWhenWebBundleIdNotTrusted) {
  IsolatedWebAppUrlInfo url_info = WriteBundleToDisk();
  SetTrustedWebBundleIdsForTesting({});
  FakeWebContentsManager& fake_web_contents_manager =
      static_cast<FakeWebContentsManager&>(
          fake_provider().web_contents_manager());
  MockIconAndPageState(fake_web_contents_manager, url_info);

  base::test::TestFuture<base::expected<SignedWebBundleMetadata, std::string>>
      metadata_future;
  SignedWebBundleMetadata::Create(profile(), &fake_provider(), url_info,
                                  bundle_source(),
                                  metadata_future.GetCallback());
  base::expected<SignedWebBundleMetadata, std::string> metadata =
      metadata_future.Get();

  EXPECT_THAT(metadata, ErrorIs(HasSubstr("not trusted")));
}

TEST_F(SignedWebBundleMetadataTest, FailsWhenBundleInvalid) {
  IsolatedWebAppUrlInfo url_info = WriteBundleToDisk(
      TestSignedWebBundleBuilder::BuildOptions().SetErrorsForTesting(
          {{web_package::test::WebBundleSigner::IntegrityBlockErrorForTesting::
                kInvalidIntegrityBlockStructure},
           {}}));
  SetTrustedWebBundleIdsForTesting({url_info.web_bundle_id()});
  FakeWebContentsManager& fake_web_contents_manager =
      static_cast<FakeWebContentsManager&>(
          fake_provider().web_contents_manager());
  MockIconAndPageState(fake_web_contents_manager, url_info);

  base::test::TestFuture<base::expected<SignedWebBundleMetadata, std::string>>
      metadata_future;
  SignedWebBundleMetadata::Create(profile(), &fake_provider(), url_info,
                                  bundle_source(),
                                  metadata_future.GetCallback());
  base::expected<SignedWebBundleMetadata, std::string> metadata =
      metadata_future.Get();

  EXPECT_THAT(
      metadata,
      ErrorIs(HasSubstr("Integrity block array of length 6 - should be 4.")));
}

}  // namespace
}  // namespace web_app
