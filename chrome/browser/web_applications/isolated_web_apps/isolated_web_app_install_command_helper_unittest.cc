// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_install_command_helper.h"

#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_piece.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_builder.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/error/unusable_swbn_file_error.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_location.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_response_reader_factory.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_validator.h"
#include "chrome/browser/web_applications/isolated_web_apps/pending_install_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_fake_response_reader_factory.h"
#include "chrome/browser/web_applications/test/mock_data_retriever.h"
#include "chrome/browser/web_applications/test/test_web_app_url_loader.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_contents/web_app_url_loader.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-shared.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

namespace web_app {
namespace {

using ::base::test::ErrorIs;
using ::base::test::HasValue;
using ::base::test::IsNotNullCallback;
using ::base::test::RunOnceCallback;
using ::base::test::ValueIs;
using ::testing::_;
using ::testing::DoAll;
using ::testing::Each;
using ::testing::Eq;
using ::testing::Field;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::IsFalse;
using ::testing::IsNull;
using ::testing::IsTrue;
using ::testing::NiceMock;
using ::testing::Not;
using ::testing::NotNull;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::ResultOf;
using ::testing::UnorderedElementsAre;
using ::testing::VariantWith;
using ::testing::WithArg;

IsolatedWebAppUrlInfo CreateRandomIsolatedWebAppUrlInfo() {
  web_package::SignedWebBundleId signed_web_bundle_id =
      web_package::SignedWebBundleId::CreateRandomForDevelopment();
  return IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
      signed_web_bundle_id);
}

IsolatedWebAppUrlInfo CreateEd25519IsolatedWebAppUrlInfo() {
  web_package::SignedWebBundleId signed_web_bundle_id =
      web_package::SignedWebBundleId::CreateForEd25519PublicKey(
          web_package::Ed25519PublicKey::Create(
              base::make_span(kTestPublicKey)));
  return IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
      signed_web_bundle_id);
}

IsolatedWebAppLocation CreateDevProxyLocation(
    base::StringPiece dev_mode_proxy_url = "http://default-proxy-url.org/") {
  return DevModeProxy{.proxy_url =
                          url::Origin::Create(GURL(dev_mode_proxy_url))};
}

blink::mojom::ManifestPtr CreateDefaultManifest(const GURL& application_url) {
  auto manifest = blink::mojom::Manifest::New();
  manifest->id = application_url.DeprecatedGetOriginAsURL();
  manifest->scope = application_url.Resolve("/");
  manifest->start_url = application_url.Resolve("/testing-start-url.html");
  manifest->display = DisplayMode::kStandalone;
  manifest->short_name = u"test short manifest name";
  manifest->version = u"1.0.0";
  return manifest;
}

GURL CreateDefaultManifestURL(const GURL& application_url) {
  return application_url.Resolve("/manifest.webmanifest");
}

auto ReturnManifest(const blink::mojom::ManifestPtr& manifest,
                    const GURL& manifest_url,
                    webapps::InstallableStatusCode error_code =
                        webapps::InstallableStatusCode::NO_ERROR_DETECTED) {
  constexpr int kCallbackArgumentIndex = 1;

  return DoAll(
      WithArg<kCallbackArgumentIndex>(
          [](const WebAppDataRetriever::CheckInstallabilityCallback& callback) {
            DCHECK(!callback.is_null());
          }),
      RunOnceCallback<kCallbackArgumentIndex>(
          /*manifest=*/manifest.Clone(),
          /*manifest_url=*/manifest_url,
          /*valid_manifest_for_web_app=*/true, error_code));
}

std::unique_ptr<MockDataRetriever> CreateDefaultDataRetriever(
    const GURL& application_url) {
  std::unique_ptr<MockDataRetriever> fake_data_retriever =
      std::make_unique<NiceMock<MockDataRetriever>>();

  EXPECT_CALL(*fake_data_retriever, GetWebAppInstallInfo).Times(0);

  return fake_data_retriever;
}

class IsolatedWebAppInstallCommandHelperTest : public ::testing::Test {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {features::kIsolatedWebApps, features::kIsolatedWebAppDevMode}, {});
  }

  TestingProfile* profile() const { return profile_.get(); }

  content::WebContents& web_contents() {
    if (web_contents_ == nullptr) {
      web_contents_ = content::WebContents::Create(
          content::WebContents::CreateParams(profile()));
    }
    return *web_contents_;
  }

 private:
  content::BrowserTaskEnvironment browser_task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<TestingProfile> profile_ = []() {
    TestingProfile::Builder builder;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
    builder.SetIsMainProfile(true);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

    return builder.Build();
  }();
  std::unique_ptr<content::WebContents> web_contents_;
};

using IsolatedWebAppInstallCommandHelperTrustAndSignaturesTest =
    IsolatedWebAppInstallCommandHelperTest;

TEST_F(IsolatedWebAppInstallCommandHelperTrustAndSignaturesTest,
       DevProxySucceeds) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  auto command_helper = std::make_unique<IsolatedWebAppInstallCommandHelper>(
      url_info, CreateDefaultDataRetriever(url_info.origin().GetURL()),
      /*response_reader_factory=*/nullptr);

  base::test::TestFuture<base::expected<void, std::string>> future;
  command_helper->CheckTrustAndSignatures(CreateDevProxyLocation(), &*profile(),
                                          future.GetCallback());
  EXPECT_THAT(future.Get(), HasValue());
}

TEST_F(IsolatedWebAppInstallCommandHelperTrustAndSignaturesTest,
       DevProxyFailsWhenDevModeIsDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kIsolatedWebAppDevMode);

  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  auto command_helper = std::make_unique<IsolatedWebAppInstallCommandHelper>(
      url_info, CreateDefaultDataRetriever(url_info.origin().GetURL()),
      /*response_reader_factory=*/nullptr);

  base::test::TestFuture<base::expected<void, std::string>> future;
  command_helper->CheckTrustAndSignatures(CreateDevProxyLocation(), &*profile(),
                                          future.GetCallback());
  EXPECT_THAT(
      future.Take(),
      ErrorIs(HasSubstr("Isolated Web App Developer Mode is not enabled")));
}

class IsolatedWebAppInstallCommandHelperTrustAndSignaturesBundleTest
    : public IsolatedWebAppInstallCommandHelperTrustAndSignaturesTest,
      public ::testing::WithParamInterface<bool> {
 public:
  IsolatedWebAppInstallCommandHelperTrustAndSignaturesBundleTest()
      : is_dev_mode_(GetParam()),
        location_(is_dev_mode_ ? IsolatedWebAppLocation(InstalledBundle{
                                     .path = base::FilePath{FILE_PATH_LITERAL(
                                         "/testing/path/to/a/bundle")},
                                 })
                               : IsolatedWebAppLocation(DevModeBundle{
                                     .path = base::FilePath{FILE_PATH_LITERAL(
                                         "/testing/path/to/a/bundle")},
                                 })) {}

 protected:
  bool is_dev_mode_;
  IsolatedWebAppLocation location_;
};

TEST_P(IsolatedWebAppInstallCommandHelperTrustAndSignaturesBundleTest,
       SucceedsWhenThereIsNoError) {
  IsolatedWebAppUrlInfo url_info = CreateEd25519IsolatedWebAppUrlInfo();
  auto command_helper = std::make_unique<IsolatedWebAppInstallCommandHelper>(
      url_info, CreateDefaultDataRetriever(url_info.origin().GetURL()),
      std::make_unique<FakeResponseReaderFactory>(base::ok()));

  base::test::TestFuture<base::expected<void, std::string>> future;
  command_helper->CheckTrustAndSignatures(location_, &*profile(),
                                          future.GetCallback());
  EXPECT_THAT(future.Get(), HasValue());
}

TEST_P(IsolatedWebAppInstallCommandHelperTrustAndSignaturesBundleTest,
       ErrorsOnBundleError) {
  IsolatedWebAppUrlInfo url_info = CreateEd25519IsolatedWebAppUrlInfo();
  auto command_helper = std::make_unique<IsolatedWebAppInstallCommandHelper>(
      url_info, CreateDefaultDataRetriever(url_info.origin().GetURL()),
      std::make_unique<FakeResponseReaderFactory>(
          base::unexpected(UnusableSwbnFileError(
              UnusableSwbnFileError::Error::kMetadataParserVersionError,
              "test error"))));

  base::test::TestFuture<base::expected<void, std::string>> future;
  command_helper->CheckTrustAndSignatures(location_, &*profile(),
                                          future.GetCallback());
  EXPECT_THAT(future.Take(), ErrorIs(HasSubstr("test error")));
}

TEST_P(IsolatedWebAppInstallCommandHelperTrustAndSignaturesBundleTest,
       DoesNotInstallDevModeBundleWhenDevModeIsDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kIsolatedWebAppDevMode);

  IsolatedWebAppUrlInfo url_info = CreateEd25519IsolatedWebAppUrlInfo();
  auto command_helper = std::make_unique<IsolatedWebAppInstallCommandHelper>(
      url_info, CreateDefaultDataRetriever(url_info.origin().GetURL()),
      std::make_unique<FakeResponseReaderFactory>(base::ok()));
  base::test::TestFuture<base::expected<void, std::string>> future;
  command_helper->CheckTrustAndSignatures(location_, &*profile(),
                                          future.GetCallback());
  if (GetParam()) {
    EXPECT_THAT(future.Get(), HasValue());
  } else {
    EXPECT_THAT(
        future.Take(),
        ErrorIs(HasSubstr("Isolated Web App Developer Mode is not enabled")));
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    IsolatedWebAppInstallCommandHelperTrustAndSignaturesBundleTest,
    ::testing::Bool(),
    [](::testing::TestParamInfo<bool> param_info) {
      return param_info.param ? "DevModeBundle" : "InstalledBundle";
    });

using IsolatedWebAppInstallCommandHelperStoragePartitionTest =
    IsolatedWebAppInstallCommandHelperTest;

TEST_F(IsolatedWebAppInstallCommandHelperStoragePartitionTest,
       CreateIfNotPresent) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  auto command_helper = std::make_unique<IsolatedWebAppInstallCommandHelper>(
      url_info, CreateDefaultDataRetriever(url_info.origin().GetURL()),
      /*response_reader_factory=*/nullptr);

  command_helper->CreateStoragePartitionIfNotPresent(*profile());
  EXPECT_THAT(profile()->GetStoragePartition(
                  url_info.storage_partition_config(profile()),
                  /*can_create=*/false),
              NotNull());
}

TEST_F(IsolatedWebAppInstallCommandHelperStoragePartitionTest,
       CreateIfPresent) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  auto command_helper = std::make_unique<IsolatedWebAppInstallCommandHelper>(
      url_info, CreateDefaultDataRetriever(url_info.origin().GetURL()),
      /*response_reader_factory=*/nullptr);

  auto* partition = profile()->GetStoragePartition(
      url_info.storage_partition_config(profile()),
      /*can_create=*/true);

  command_helper->CreateStoragePartitionIfNotPresent(*profile());
  EXPECT_THAT(profile()->GetStoragePartition(
                  url_info.storage_partition_config(profile()),
                  /*can_create=*/false),
              Eq(partition));
}

using IsolatedWebAppInstallCommandHelperLoadUrlTest =
    IsolatedWebAppInstallCommandHelperTest;

TEST_F(IsolatedWebAppInstallCommandHelperLoadUrlTest,
       URLLoaderIsCalledWithUrlGivenToTheInstallCommand) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  auto command_helper = std::make_unique<IsolatedWebAppInstallCommandHelper>(
      url_info, CreateDefaultDataRetriever(url_info.origin().GetURL()),
      /*response_reader_factory=*/nullptr);

  auto url_loader = std::make_unique<TestWebAppUrlLoader>();
  url_loader->SetNextLoadUrlResult(
      url_info.origin().GetURL().Resolve(
          ".well-known/_generated_install_page.html"),
      WebAppUrlLoader::Result::kUrlLoaded);

  absl::optional<WebAppUrlLoader::UrlComparison> last_url_comparison =
      absl::nullopt;
  url_loader->TrackLoadUrlCalls(base::BindLambdaForTesting(
      [&](const GURL& unused_url, content::WebContents* unused_web_contents,
          WebAppUrlLoader::UrlComparison url_comparison) {
        last_url_comparison = url_comparison;
      }));

  base::test::TestFuture<base::expected<void, std::string>> future;
  command_helper->LoadInstallUrl(CreateDevProxyLocation(), web_contents(),
                                 *url_loader, future.GetCallback());
  EXPECT_THAT(future.Get(), HasValue());
  EXPECT_THAT(last_url_comparison,
              Eq(WebAppUrlLoader::UrlComparison::kIgnoreQueryParamsAndRef));
}

TEST_F(IsolatedWebAppInstallCommandHelperLoadUrlTest,
       SetDevModeLocationBeforeUrlLoading) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  auto command_helper = std::make_unique<IsolatedWebAppInstallCommandHelper>(
      url_info, CreateDefaultDataRetriever(url_info.origin().GetURL()),
      /*response_reader_factory=*/nullptr);

  auto url_loader = std::make_unique<TestWebAppUrlLoader>();
  url_loader->SetNextLoadUrlResult(
      url_info.origin().GetURL().Resolve(
          ".well-known/_generated_install_page.html"),
      WebAppUrlLoader::Result::kUrlLoaded);

  absl::optional<IsolatedWebAppLocation> location = absl::nullopt;
  url_loader->TrackLoadUrlCalls(base::BindLambdaForTesting(
      [&](const GURL& unused_url, content::WebContents* web_contents,
          WebAppUrlLoader::UrlComparison unused_url_comparison) {
        location =
            IsolatedWebAppPendingInstallInfo::FromWebContents(*web_contents)
                .location();
      }));

  base::test::TestFuture<base::expected<void, std::string>> future;
  command_helper->LoadInstallUrl(
      DevModeProxy{.proxy_url = url::Origin::Create(
                       GURL("http://some-testing-proxy-url.com/"))},
      web_contents(), *url_loader, future.GetCallback());
  EXPECT_THAT(future.Get(), HasValue());
  EXPECT_THAT(location, Optional(VariantWith<DevModeProxy>(Field(
                            "proxy_url", &DevModeProxy::proxy_url,
                            Eq(url::Origin::Create(GURL(
                                "http://some-testing-proxy-url.com/")))))));
}

TEST_F(IsolatedWebAppInstallCommandHelperLoadUrlTest,
       SetInstalledBundleLocationBeforeUrlLoading) {
  IsolatedWebAppUrlInfo url_info = CreateEd25519IsolatedWebAppUrlInfo();
  auto command_helper = std::make_unique<IsolatedWebAppInstallCommandHelper>(
      url_info, CreateDefaultDataRetriever(url_info.origin().GetURL()),
      /*response_reader_factory=*/nullptr);

  auto url_loader = std::make_unique<TestWebAppUrlLoader>();
  url_loader->SetNextLoadUrlResult(
      url_info.origin().GetURL().Resolve(
          ".well-known/_generated_install_page.html"),
      WebAppUrlLoader::Result::kUrlLoaded);

  absl::optional<IsolatedWebAppLocation> location = absl::nullopt;
  url_loader->TrackLoadUrlCalls(base::BindLambdaForTesting(
      [&](const GURL& unused_url, content::WebContents* web_contents,
          WebAppUrlLoader::UrlComparison unused_url_comparison) {
        location =
            IsolatedWebAppPendingInstallInfo::FromWebContents(*web_contents)
                .location();
      }));

  base::test::TestFuture<base::expected<void, std::string>> future;
  command_helper->LoadInstallUrl(
      InstalledBundle{
          .path =
              base::FilePath{FILE_PATH_LITERAL("/testing/path/to/a/bundle")},
      },
      web_contents(), *url_loader, future.GetCallback());
  EXPECT_THAT(future.Get(), HasValue());
  EXPECT_THAT(location, Optional(VariantWith<InstalledBundle>(
                            Field("path", &InstalledBundle::path,
                                  Eq(base::FilePath{FILE_PATH_LITERAL(
                                      "/testing/path/to/a/bundle")})))));
}

TEST_F(IsolatedWebAppInstallCommandHelperLoadUrlTest, HandlesFailure) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  auto command_helper = std::make_unique<IsolatedWebAppInstallCommandHelper>(
      url_info, CreateDefaultDataRetriever(url_info.origin().GetURL()),
      /*response_reader_factory=*/nullptr);

  auto url_loader = std::make_unique<TestWebAppUrlLoader>();
  url_loader->SetNextLoadUrlResult(
      url_info.origin().GetURL().Resolve(
          ".well-known/_generated_install_page.html"),
      WebAppUrlLoader::Result::kFailedErrorPageLoaded);

  base::test::TestFuture<base::expected<void, std::string>> future;
  command_helper->LoadInstallUrl(CreateDevProxyLocation(), web_contents(),
                                 *url_loader, future.GetCallback());
  EXPECT_THAT(future.Get(), ErrorIs(HasSubstr("FailedErrorPageLoaded")));
}

using IsolatedWebAppInstallCommandHelperRetrieveManifestTest =
    IsolatedWebAppInstallCommandHelperTest;

TEST_F(IsolatedWebAppInstallCommandHelperRetrieveManifestTest,
       ServiceWorkerIsNotRequired) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();

  std::unique_ptr<MockDataRetriever> fake_data_retriever =
      CreateDefaultDataRetriever(url_info.origin().GetURL());
  EXPECT_CALL(*fake_data_retriever,
              CheckInstallabilityAndRetrieveManifest(_, _, _))
      .WillOnce(
          ReturnManifest(CreateDefaultManifest(url_info.origin().GetURL()),
                         CreateDefaultManifestURL(url_info.origin().GetURL())));
  auto command_helper = std::make_unique<IsolatedWebAppInstallCommandHelper>(
      url_info, std::move(fake_data_retriever),
      /*response_reader_factory=*/nullptr);

  base::test::TestFuture<base::expected<
      IsolatedWebAppInstallCommandHelper::ManifestAndUrl, std::string>>
      future;
  command_helper->CheckInstallabilityAndRetrieveManifest(web_contents(),
                                                         future.GetCallback());
  EXPECT_THAT(future.Get(), HasValue());
}

TEST_F(IsolatedWebAppInstallCommandHelperRetrieveManifestTest,
       FailsWhenAppIsNotInstallable) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();

  std::unique_ptr<MockDataRetriever> fake_data_retriever =
      CreateDefaultDataRetriever(url_info.origin().GetURL());
  ON_CALL(*fake_data_retriever, CheckInstallabilityAndRetrieveManifest)
      .WillByDefault(
          ReturnManifest(blink::mojom::Manifest::New(),
                         GURL{"http://test-url-example.com/manifest.json"},
                         webapps::InstallableStatusCode::NO_MANIFEST));
  auto command_helper = std::make_unique<IsolatedWebAppInstallCommandHelper>(
      url_info, std::move(fake_data_retriever),
      /*response_reader_factory=*/nullptr);

  base::test::TestFuture<base::expected<
      IsolatedWebAppInstallCommandHelper::ManifestAndUrl, std::string>>
      future;
  command_helper->CheckInstallabilityAndRetrieveManifest(web_contents(),
                                                         future.GetCallback());
  EXPECT_THAT(future.Take(), ErrorIs(HasSubstr("App is not installable")));
}

TEST_F(IsolatedWebAppInstallCommandHelperRetrieveManifestTest,
       FailsWhenAppIsInstallableButManifestIsNull) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();

  std::unique_ptr<MockDataRetriever> fake_data_retriever =
      CreateDefaultDataRetriever(url_info.origin().GetURL());
  ON_CALL(*fake_data_retriever, CheckInstallabilityAndRetrieveManifest)
      .WillByDefault(ReturnManifest(
          /*manifest=*/nullptr,
          CreateDefaultManifestURL(url_info.origin().GetURL())));
  auto command_helper = std::make_unique<IsolatedWebAppInstallCommandHelper>(
      url_info, std::move(fake_data_retriever),
      /*response_reader_factory=*/nullptr);
  base::test::TestFuture<base::expected<
      IsolatedWebAppInstallCommandHelper::ManifestAndUrl, std::string>>
      future;
  command_helper->CheckInstallabilityAndRetrieveManifest(web_contents(),
                                                         future.GetCallback());
  EXPECT_THAT(future.Take(), ErrorIs(HasSubstr("Manifest is null")));
}

struct InvalidVersionParam {
  absl::optional<std::u16string> version;
  std::string error;
  std::string test_name;
};

using IsolatedWebAppInstallCommandHelperValidateManifestTest =
    IsolatedWebAppInstallCommandHelperTest;

class InstallIsolatedWebAppCommandHelperInvalidVersionTest
    : public IsolatedWebAppInstallCommandHelperValidateManifestTest,
      public ::testing::WithParamInterface<InvalidVersionParam> {};

TEST_P(InstallIsolatedWebAppCommandHelperInvalidVersionTest,
       InstallationFails) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  auto command_helper = std::make_unique<IsolatedWebAppInstallCommandHelper>(
      url_info, CreateDefaultDataRetriever(url_info.origin().GetURL()),
      /*response_reader_factory=*/nullptr);

  std::unique_ptr<MockDataRetriever> fake_data_retriever =
      CreateDefaultDataRetriever(url_info.origin().GetURL());
  auto manifest = CreateDefaultManifest(url_info.origin().GetURL());
  manifest->version = GetParam().version;

  IsolatedWebAppInstallCommandHelper::ManifestAndUrl manifest_and_url(
      std::move(manifest),
      CreateDefaultManifestURL(url_info.origin().GetURL()));

  base::expected<WebAppInstallInfo, std::string> result =
      command_helper->ValidateManifestAndCreateInstallInfo(
          /*expected_version=*/absl::nullopt, std::move(manifest_and_url));
  EXPECT_THAT(result, ErrorIs(HasSubstr(GetParam().error)));
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    InstallIsolatedWebAppCommandHelperInvalidVersionTest,
    ::testing::Values(
        InvalidVersionParam{.version = absl::nullopt,
                            .error = "`version` is not present",
                            .test_name = "NoVersion"},
        InvalidVersionParam{
            .version = u"\xD801",
            .error = "Failed to convert manifest `version` from UTF16 to UTF8",
            .test_name = "InvalidUtf8"},
        InvalidVersionParam{.version = u"10abc",
                            .error = "Failed to parse `version`",
                            .test_name = "InvalidVersionFormat"}),
    [](const ::testing::TestParamInfo<
        InstallIsolatedWebAppCommandHelperInvalidVersionTest::ParamType>&
           info) { return info.param.test_name; });

TEST_F(IsolatedWebAppInstallCommandHelperValidateManifestTest,
       FailsWhenAppVersionDoesNotMatchExpectedVersion) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  auto command_helper = std::make_unique<IsolatedWebAppInstallCommandHelper>(
      url_info, CreateDefaultDataRetriever(url_info.origin().GetURL()),
      /*response_reader_factory=*/nullptr);

  base::expected<WebAppInstallInfo, std::string> result =
      command_helper->ValidateManifestAndCreateInstallInfo(
          base::Version("99.99.99"),
          IsolatedWebAppInstallCommandHelper::ManifestAndUrl(
              CreateDefaultManifest(url_info.origin().GetURL()),
              CreateDefaultManifestURL(url_info.origin().GetURL())));
  EXPECT_THAT(result,
              ErrorIs(HasSubstr(
                  "does not match the version provided in the manifest")));
}

TEST_F(IsolatedWebAppInstallCommandHelperValidateManifestTest,
       SucceedsWhenManifestIdIsEmpty) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  auto command_helper = std::make_unique<IsolatedWebAppInstallCommandHelper>(
      url_info, CreateDefaultDataRetriever(url_info.origin().GetURL()),
      /*response_reader_factory=*/nullptr);

  base::expected<WebAppInstallInfo, std::string> result =
      command_helper->ValidateManifestAndCreateInstallInfo(
          /*expected_version=*/absl::nullopt,
          IsolatedWebAppInstallCommandHelper::ManifestAndUrl(
              CreateDefaultManifest(url_info.origin().GetURL()),
              CreateDefaultManifestURL(url_info.origin().GetURL())));
  EXPECT_THAT(result, HasValue());
}

TEST_F(IsolatedWebAppInstallCommandHelperValidateManifestTest,
       FailsWhenManifestIdIsNotEmpty) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  auto command_helper = std::make_unique<IsolatedWebAppInstallCommandHelper>(
      url_info, CreateDefaultDataRetriever(url_info.origin().GetURL()),
      /*response_reader_factory=*/nullptr);

  blink::mojom::ManifestPtr manifest =
      CreateDefaultManifest(url_info.origin().GetURL());
  manifest->id = url_info.origin().GetURL().Resolve("/test-manifest-id");

  base::expected<WebAppInstallInfo, std::string> result =
      command_helper->ValidateManifestAndCreateInstallInfo(
          /*expected_version=*/absl::nullopt,
          IsolatedWebAppInstallCommandHelper::ManifestAndUrl(
              std::move(manifest),
              CreateDefaultManifestURL(url_info.origin().GetURL())));
  EXPECT_THAT(result, ErrorIs(HasSubstr(R"(Manifest `id` must be "/")")));
}

TEST_F(IsolatedWebAppInstallCommandHelperValidateManifestTest,
       FailsWhenManifestScopeIsNotSlash) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  auto command_helper = std::make_unique<IsolatedWebAppInstallCommandHelper>(
      url_info, CreateDefaultDataRetriever(url_info.origin().GetURL()),
      /*response_reader_factory=*/nullptr);

  blink::mojom::ManifestPtr manifest =
      CreateDefaultManifest(url_info.origin().GetURL());
  manifest->scope = url_info.origin().GetURL().Resolve("/scope");

  base::expected<WebAppInstallInfo, std::string> result =
      command_helper->ValidateManifestAndCreateInstallInfo(
          /*expected_version=*/absl::nullopt,
          IsolatedWebAppInstallCommandHelper::ManifestAndUrl(
              std::move(manifest),
              CreateDefaultManifestURL(url_info.origin().GetURL())));
  EXPECT_THAT(result, ErrorIs(HasSubstr("Scope should resolve to the origin")));
}

TEST_F(IsolatedWebAppInstallCommandHelperValidateManifestTest,
       ScopeIsResolvedToRootWhenManifestScopeIsSlash) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  auto command_helper = std::make_unique<IsolatedWebAppInstallCommandHelper>(
      url_info, CreateDefaultDataRetriever(url_info.origin().GetURL()),
      /*response_reader_factory=*/nullptr);

  blink::mojom::ManifestPtr manifest =
      CreateDefaultManifest(url_info.origin().GetURL());
  manifest->scope = url_info.origin().GetURL().Resolve("/");

  base::expected<WebAppInstallInfo, std::string> result =
      command_helper->ValidateManifestAndCreateInstallInfo(
          /*expected_version=*/absl::nullopt,
          IsolatedWebAppInstallCommandHelper::ManifestAndUrl(
              std::move(manifest),
              CreateDefaultManifestURL(url_info.origin().GetURL())));
  EXPECT_THAT(result, ValueIs(Field(&WebAppInstallInfo::scope,
                                    Eq(url_info.origin().GetURL()))));
}

TEST_F(IsolatedWebAppInstallCommandHelperValidateManifestTest,
       UntranslatedNameIsEmptyWhenNameAndShortNameAreNotPresent) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  auto command_helper = std::make_unique<IsolatedWebAppInstallCommandHelper>(
      url_info, CreateDefaultDataRetriever(url_info.origin().GetURL()),
      /*response_reader_factory=*/nullptr);

  blink::mojom::ManifestPtr manifest =
      CreateDefaultManifest(url_info.origin().GetURL());
  manifest->name = absl::nullopt;
  manifest->short_name = absl::nullopt;

  base::expected<WebAppInstallInfo, std::string> result =
      command_helper->ValidateManifestAndCreateInstallInfo(
          /*expected_version=*/absl::nullopt,
          IsolatedWebAppInstallCommandHelper::ManifestAndUrl(
              std::move(manifest),
              CreateDefaultManifestURL(url_info.origin().GetURL())));
  EXPECT_THAT(result,
              ErrorIs(HasSubstr(
                  "App manifest must have either 'name' or 'short_name'")));
}

class InstallIsolatedWebAppCommandHelperManifestIconsTest
    : public IsolatedWebAppInstallCommandHelperTest {
 public:
 protected:
  GURL kSomeTestApplicationUrl = GURL("http://manifest-test-url.com");

  blink::mojom::ManifestPtr CreateManifest() const {
    return CreateDefaultManifest(kSomeTestApplicationUrl);
  }

  SkBitmap CreateTestBitmap(SkColor color) {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(kImageSize, kImageSize);
    bitmap.eraseColor(color);
    return bitmap;
  }

  blink::Manifest::ImageResource CreateImageResourceForAnyPurpose(
      const GURL& image_src) {
    blink::Manifest::ImageResource image;
    image.type = u"image/png";
    image.sizes.push_back(gfx::Size{kImageSize, kImageSize});
    image.purpose = {
        blink::mojom::ManifestImageResource_Purpose::ANY,
    };
    image.src = image_src;
    return image;
  }

  constexpr static int kImageSize = 96;
};

TEST_F(InstallIsolatedWebAppCommandHelperManifestIconsTest,
       ManifestIconIsDownloaded) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();

  kSomeTestApplicationUrl = url_info.origin().GetURL();
  GURL img_url = url_info.origin().GetURL().Resolve("icon.png");

  blink::mojom::ManifestPtr manifest = CreateManifest();
  manifest->icons = {CreateImageResourceForAnyPurpose(img_url)};

  std::map<GURL, std::vector<SkBitmap>> icons = {{
      img_url,
      {CreateTestBitmap(SK_ColorRED)},
  }};

  using HttpStatusCode = int;
  std::map<GURL, HttpStatusCode> http_result = {
      {img_url, net::HttpStatusCode::HTTP_OK},
  };

  std::unique_ptr<MockDataRetriever> fake_data_retriever =
      CreateDefaultDataRetriever(kSomeTestApplicationUrl);
  EXPECT_CALL(*fake_data_retriever,
              GetIcons(_, UnorderedElementsAre(img_url),
                       /*skip_page_favicons=*/true,
                       /*fail_all_if_any_fail=*/true, IsNotNullCallback()))
      .WillOnce(RunOnceCallback<4>(IconsDownloadedResult::kCompleted,
                                   std::move(icons), http_result));
  auto command_helper = std::make_unique<IsolatedWebAppInstallCommandHelper>(
      url_info, std::move(fake_data_retriever),
      /*response_reader_factory=*/nullptr);

  ASSERT_OK_AND_ASSIGN(
      auto install_info,
      command_helper->ValidateManifestAndCreateInstallInfo(
          absl::nullopt,
          IsolatedWebAppInstallCommandHelper::ManifestAndUrl(
              std::move(manifest),
              CreateDefaultManifestURL(kSomeTestApplicationUrl))));

  base::test::TestFuture<base::expected<WebAppInstallInfo, std::string>> future;
  command_helper->RetrieveIconsAndPopulateInstallInfo(
      std::move(install_info), web_contents(), future.GetCallback());
  auto result = future.Take();
  EXPECT_THAT(result, HasValue());

  std::map<SquareSizePx, SkBitmap> icon_bitmaps = result->icon_bitmaps.any;
  EXPECT_THAT(result, ValueIs(Field(
                          &WebAppInstallInfo::icon_bitmaps,
                          Field(&IconBitmaps::any,
                                Each(Pair(_, ResultOf(
                                                 "bitmap.color.at.0.0",
                                                 [](const SkBitmap& bitmap) {
                                                   return bitmap.getColor(0, 0);
                                                 },
                                                 Eq(SK_ColorRED))))))));

  EXPECT_THAT(
      result,
      ValueIs(Field(
          "manifest_icons", &WebAppInstallInfo::manifest_icons,
          UnorderedElementsAre(Field(&apps::IconInfo::url, Eq(img_url))))));
}

TEST_F(InstallIsolatedWebAppCommandHelperManifestIconsTest,
       InstallationFailsWhenIconDownloadingFails) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();

  kSomeTestApplicationUrl = url_info.origin().GetURL();
  GURL img_url = url_info.origin().GetURL().Resolve("icon.png");

  blink::mojom::ManifestPtr manifest = CreateManifest();
  manifest->icons = {CreateImageResourceForAnyPurpose(img_url)};

  std::map<GURL, std::vector<SkBitmap>> icons = {};

  using HttpStatusCode = int;
  std::map<GURL, HttpStatusCode> http_result = {};

  std::unique_ptr<MockDataRetriever> fake_data_retriever =
      CreateDefaultDataRetriever(url_info.origin().GetURL());
  EXPECT_CALL(*fake_data_retriever, GetIcons(_, _, _, _, IsNotNullCallback()))
      .WillOnce(RunOnceCallback<4>(IconsDownloadedResult::kAbortedDueToFailure,
                                   std::move(icons), http_result));
  auto command_helper = std::make_unique<IsolatedWebAppInstallCommandHelper>(
      url_info, std::move(fake_data_retriever),
      /*response_reader_factory=*/nullptr);

  ASSERT_OK_AND_ASSIGN(
      auto install_info,
      command_helper->ValidateManifestAndCreateInstallInfo(
          absl::nullopt,
          IsolatedWebAppInstallCommandHelper::ManifestAndUrl(
              std::move(manifest),
              CreateDefaultManifestURL(kSomeTestApplicationUrl))));

  base::test::TestFuture<base::expected<WebAppInstallInfo, std::string>> future;
  command_helper->RetrieveIconsAndPopulateInstallInfo(
      std::move(install_info), web_contents(), future.GetCallback());
  auto result = future.Take();
  EXPECT_THAT(
      result,
      ErrorIs(HasSubstr("Error during icon downloading: AbortedDueToFailure")));
}

struct VerifyRelocationVisitor {
  explicit VerifyRelocationVisitor(base::FilePath profile_dir,
                                   base::FilePath source_path)
      : profile_dir_(std::move(profile_dir)),
        source_path_(std::move(source_path)) {}

  void operator()(const InstalledBundle& location) {
    // Check that the bundle was copied to the profile's IWA directory.
    EXPECT_TRUE(base::PathExists(location.path));
    EXPECT_TRUE(base::PathExists(source_path_));
    EXPECT_EQ(location.path.DirName().DirName(),
              profile_dir_.Append(kIwaDirName));
    EXPECT_EQ(location.path.BaseName(), base::FilePath(kMainSwbnFileName));
  }

  void operator()(const DevModeBundle& location) {
    // Dev mode bundle should not be relocated.
    EXPECT_EQ(location.path, source_path_);
    EXPECT_TRUE(base::PathExists(location.path));
  }

  void operator()(const DevModeProxy& location) {}

 private:
  base::FilePath profile_dir_;
  base::FilePath source_path_;
};

struct VerifyCleanupVisitor {
  void operator()(const InstalledBundle& location) {
    // The copied to profile directory bundles should be deleted on cleanup.
    EXPECT_FALSE(base::PathExists(location.path));
  }

  void operator()(const DevModeBundle& location) {
    // Dev mode bundle are not copied to the profile dir and because of that
    // should not be deleted.
    EXPECT_TRUE(base::PathExists(location.path));
  }

  void operator()(const DevModeProxy& location) {}
};

TEST(InstallIsolatedWebAppCommandHelperRelocationTest, NormalFlow) {
  using RelocationResult = base::expected<IsolatedWebAppLocation, std::string>;
  base::test::TaskEnvironment task_environment;

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath profile_dir;
  ASSERT_TRUE(base::CreateTemporaryDirInDir(
      temp_dir.GetPath(), FILE_PATH_LITERAL("profile"), &profile_dir));

  // A directory where source files are stored.
  base::FilePath src_dir;
  ASSERT_TRUE(base::CreateTemporaryDirInDir(
      temp_dir.GetPath(), FILE_PATH_LITERAL("src"), &src_dir));

  // Installed bundle case.
  {
    base::FilePath src_installed_bundle;
    ASSERT_TRUE(base::CreateTemporaryFileInDir(src_dir, &src_installed_bundle));

    IsolatedWebAppLocation installed_bundle_location =
        InstalledBundle{.path = src_installed_bundle};

    // Check that relocation works.
    base::test::TestFuture<RelocationResult> future;
    CopyLocationToProfileDirectory(profile_dir, installed_bundle_location,
                                   future.GetCallback());
    RelocationResult result = future.Take();
    EXPECT_TRUE(result.has_value());
    absl::visit(VerifyRelocationVisitor{profile_dir, src_installed_bundle},
                result.value());

    // Check that cleanup works.
    base::test::TestFuture<void> cleanup_future;
    CleanupLocationIfOwned(profile_dir, result.value(),
                           cleanup_future.GetCallback());
    ASSERT_TRUE(cleanup_future.Wait());
    absl::visit(VerifyCleanupVisitor{}, result.value());
  }

  // Dev mode bundle case.
  {
    base::FilePath src_dev_mode_bundle;
    ASSERT_TRUE(base::CreateTemporaryFileInDir(src_dir, &src_dev_mode_bundle));

    IsolatedWebAppLocation dev_mode_location =
        DevModeBundle{.path = src_dev_mode_bundle};

    // Check that relocation works.
    base::test::TestFuture<RelocationResult> future;
    CopyLocationToProfileDirectory(profile_dir, dev_mode_location,
                                   future.GetCallback());
    RelocationResult result = future.Take();
    EXPECT_TRUE(result.has_value());
    absl::visit(VerifyRelocationVisitor{profile_dir, src_dev_mode_bundle},
                result.value());

    // Check that cleanup works.
    base::test::TestFuture<void> cleanup_future;
    CleanupLocationIfOwned(profile_dir, result.value(),
                           cleanup_future.GetCallback());
    ASSERT_TRUE(cleanup_future.Wait());
    absl::visit(VerifyCleanupVisitor{}, result.value());
  }
}

TEST(InstallIsolatedWebAppCommandHelperRelocationTest, CleanupNotOwned) {
  base::test::TaskEnvironment task_environment;

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath profile_dir;
  ASSERT_TRUE(base::CreateTemporaryDirInDir(
      temp_dir.GetPath(), FILE_PATH_LITERAL("profile"), &profile_dir));

  // Create a file that is not in the owned IWA directory.
  base::FilePath bundle_path;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_dir.GetPath(), &bundle_path));

  // Trying to cleanup the location that is not owned.
  IsolatedWebAppLocation location = InstalledBundle{.path = bundle_path};
  base::test::TestFuture<void> cleanup_future;
  CleanupLocationIfOwned(profile_dir, location, cleanup_future.GetCallback());
  ASSERT_TRUE(cleanup_future.Wait());

  // Not owned file should not be deleted.
  EXPECT_TRUE(base::PathExists(bundle_path));
}

}  // namespace
}  // namespace web_app
