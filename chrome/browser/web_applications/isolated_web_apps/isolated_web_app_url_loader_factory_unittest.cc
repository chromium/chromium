// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_loader_factory.h"

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "chrome/browser/web_applications/isolation_data.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/web_package/test_support/signed_web_bundles/web_bundle_signer.h"
#include "components/web_package/web_bundle_builder.h"
#include "content/public/common/content_features.h"
#include "content/public/test/simple_url_loader_test_helper.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {
namespace {

using ::testing::Eq;
using ::testing::IsFalse;
using ::testing::IsNull;
using ::testing::IsTrue;
using ::testing::NotNull;

constexpr uint8_t kTestPublicKey[] = {
    0xE4, 0xD5, 0x16, 0xC9, 0x85, 0x9A, 0xF8, 0x63, 0x56, 0xA3, 0x51,
    0x66, 0x7D, 0xBD, 0x00, 0x43, 0x61, 0x10, 0x1A, 0x92, 0xD4, 0x02,
    0x72, 0xFE, 0x2B, 0xCE, 0x81, 0xBB, 0x3B, 0x71, 0x3F, 0x2D,
};

constexpr uint8_t kTestPrivateKey[] = {
    0x1F, 0x27, 0x3F, 0x93, 0xE9, 0x59, 0x4E, 0xC7, 0x88, 0x82, 0xC7, 0x49,
    0xF8, 0x79, 0x3D, 0x8C, 0xDB, 0xE4, 0x60, 0x1C, 0x21, 0xF1, 0xD9, 0xF9,
    0xBC, 0x3A, 0xB5, 0xC7, 0x7F, 0x2D, 0x95, 0xE1,
    // public key (part of the private key)
    0xE4, 0xD5, 0x16, 0xC9, 0x85, 0x9A, 0xF8, 0x63, 0x56, 0xA3, 0x51, 0x66,
    0x7D, 0xBD, 0x00, 0x43, 0x61, 0x10, 0x1A, 0x92, 0xD4, 0x02, 0x72, 0xFE,
    0x2B, 0xCE, 0x81, 0xBB, 0x3B, 0x71, 0x3F, 0x2D};

std::unique_ptr<WebApp> CreateWebApp(const GURL& start_url) {
  AppId app_id = GenerateAppId(/*manifest_id=*/"", start_url);
  auto web_app = std::make_unique<WebApp>(app_id);
  web_app->SetScope(start_url.DeprecatedGetOriginAsURL());
  web_app->AddSource(WebAppManagement::Type::kCommandLine);
  return web_app;
}

std::unique_ptr<WebApp> CreateIsolatedWebApp(const GURL& start_url,
                                             IsolationData isolation_data) {
  auto web_app = CreateWebApp(start_url);
  web_app->SetIsolationData(isolation_data);
  web_app->SetIsLocallyInstalled(true);
  return web_app;
}

class IsolatedWebAppURLLoaderFactoryTest : public WebAppTest {
 public:
  explicit IsolatedWebAppURLLoaderFactoryTest(
      bool enable_isolated_web_apps_feature_flag = true)
      : enable_isolated_web_apps_feature_flag_(
            enable_isolated_web_apps_feature_flag) {}

  void SetUp() override {
    if (enable_isolated_web_apps_feature_flag_) {
      scoped_feature_list_.InitAndEnableFeature(features::kIsolatedWebApps);
    }

    WebAppTest::SetUp();

    provider_ = FakeWebAppProvider::Get(profile());
    provider_->Start();
  }

 protected:
  FakeWebAppProvider* provider() { return provider_; }

  void RegisterWebApp(std::unique_ptr<WebApp> web_app) {
    provider()->GetRegistrarMutable().registry().emplace(web_app->app_id(),
                                                         std::move(web_app));
  }

  void CreateFactory() {
    int dummy_frame_tree_node_id = 42;
    factory_.Bind(IsolatedWebAppURLLoaderFactory::Create(
        dummy_frame_tree_node_id, profile()));
  }

  int CreateLoaderAndRun(std::unique_ptr<network::ResourceRequest> request) {
    auto loader = network::SimpleURLLoader::Create(
        std::move(request), TRAFFIC_ANNOTATION_FOR_TESTS);

    content::SimpleURLLoaderTestHelper helper;
    loader->DownloadToString(
        factory_.get(), helper.GetCallback(),
        network::SimpleURLLoader::kMaxBoundedStringDownloadSize);

    helper.WaitForCallback();
    if (loader->ResponseInfo()) {
      response_info_ = loader->ResponseInfo()->Clone();
      response_body_ =
          helper.response_body() != nullptr ? *helper.response_body() : "";
    }
    return loader->NetError();
  }

  network::mojom::URLResponseHead* ResponseInfo() {
    return response_info_.get();
  }

  std::string ResponseBody() { return response_body_; }

  const std::string kWebBundleId =
      "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaac";
  const GURL kPrimaryUrl = GURL("isolated-app://" + kWebBundleId);

 private:
  bool enable_isolated_web_apps_feature_flag_;
  base::test::ScopedFeatureList scoped_feature_list_;

  FakeWebAppProvider* provider_;

  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  mojo::Remote<network::mojom::URLLoaderFactory> factory_;
  network::mojom::URLResponseHeadPtr response_info_;
  std::string response_body_;
};

TEST_F(IsolatedWebAppURLLoaderFactoryTest, LoadingFailsIfAppNotInstalled) {
  CreateFactory();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = kPrimaryUrl;
  EXPECT_THAT(CreateLoaderAndRun(std::move(request)), Eq(net::ERR_FAILED));
  EXPECT_THAT(ResponseInfo(), IsNull());
}

TEST_F(IsolatedWebAppURLLoaderFactoryTest, LoadingFailsIfInstalledAppNotIwa) {
  RegisterWebApp(CreateWebApp(kPrimaryUrl));

  // Verify that a PWA is installed at kPrimaryUrl's origin.
  absl::optional<AppId> installed_app =
      provider()->registrar().FindInstalledAppWithUrlInScope(kPrimaryUrl);
  EXPECT_THAT(installed_app.has_value(), IsTrue());

  CreateFactory();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = kPrimaryUrl;
  EXPECT_THAT(CreateLoaderAndRun(std::move(request)), Eq(net::ERR_FAILED));
  EXPECT_THAT(ResponseInfo(), IsNull());
}

TEST_F(IsolatedWebAppURLLoaderFactoryTest,
       LoadingFailsIfAppNotLocallyInstalled) {
  std::unique_ptr<WebApp> iwa = CreateIsolatedWebApp(
      kPrimaryUrl, IsolationData{IsolationData::DevModeProxy{
                       .proxy_url = kPrimaryUrl.spec()}});
  iwa->SetIsLocallyInstalled(false);
  RegisterWebApp(std::move(iwa));

  // Verify that a PWA is installed at kPrimaryUrl's origin.
  absl::optional<AppId> installed_app =
      provider()->registrar().FindAppWithUrlInScope(kPrimaryUrl);
  EXPECT_THAT(installed_app.has_value(), IsTrue());

  CreateFactory();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = kPrimaryUrl;
  EXPECT_THAT(CreateLoaderAndRun(std::move(request)), Eq(net::ERR_FAILED));
  EXPECT_THAT(ResponseInfo(), IsNull());
}

TEST_F(IsolatedWebAppURLLoaderFactoryTest, InstalledIwaReturnsNotFound) {
  RegisterWebApp(CreateIsolatedWebApp(kPrimaryUrl,
                                      IsolationData{IsolationData::DevModeProxy{
                                          .proxy_url = kPrimaryUrl.spec()}}));

  CreateFactory();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = kPrimaryUrl;
  EXPECT_THAT(CreateLoaderAndRun(std::move(request)),
              Eq(net::ERR_HTTP_RESPONSE_CODE_FAILURE));
  EXPECT_THAT(ResponseInfo()->headers->response_code(),
              Eq(net::HTTP_NOT_FOUND));
}

class IsolatedWebAppURLLoaderFactoryInstalledBundleTest
    : public IsolatedWebAppURLLoaderFactoryTest {
 public:
  explicit IsolatedWebAppURLLoaderFactoryInstalledBundleTest(
      bool enable_isolated_web_apps_feature_flag = true)
      : IsolatedWebAppURLLoaderFactoryTest(
            enable_isolated_web_apps_feature_flag) {}

 protected:
  void SetUp() override {
    IsolatedWebAppURLLoaderFactoryTest::SetUp();

    EXPECT_THAT(temp_dir_.CreateUniqueTempDir(), IsTrue());

    auto bundle_path = CreateSignedBundleAndWriteToDisk();
    std::unique_ptr<WebApp> iwa = CreateIsolatedWebApp(
        kPrimaryUrl, IsolationData{IsolationData::InstalledBundle{
                         .path = bundle_path.MaybeAsASCII()}});
    RegisterWebApp(std::move(iwa));
  }

  base::FilePath CreateSignedBundleAndWriteToDisk() {
    web_package::WebBundleBuilder builder;
    builder.AddPrimaryURL(kPrimaryUrl.spec());
    builder.AddExchange(kPrimaryUrl.spec(),
                        {{":status", "200"}, {"content-type", "text/html"}},
                        "Hello World");
    builder.AddExchange(kPrimaryUrl.spec() + "/invalid-status-code",
                        {{":status", "201"}, {"content-type", "text/html"}},
                        "Hello World");

    web_package::WebBundleSigner::KeyPair key_pair(kTestPublicKey,
                                                   kTestPrivateKey);
    return SignAndWriteBundleToDisk(builder.CreateBundle(), profile(), key_pair,
                                    kWebBundleId);
  }

  base::FilePath SignAndWriteBundleToDisk(
      const std::vector<uint8_t>& unsigned_bundle,
      Profile* profile,
      web_package::WebBundleSigner::KeyPair key_pair,
      const std::string web_bundle_id) {
    auto signed_bundle =
        web_package::WebBundleSigner::SignBundle(unsigned_bundle, {key_pair});

    base::FilePath web_bundle_path;
    EXPECT_THAT(CreateTemporaryFileInDir(temp_dir_.GetPath(), &web_bundle_path),
                IsTrue());
    EXPECT_THAT(
        static_cast<size_t>(base::WriteFile(
            web_bundle_path, reinterpret_cast<char*>(signed_bundle.data()),
            signed_bundle.size())),
        Eq(signed_bundle.size()));

    return web_bundle_path;
  }

  base::ScopedTempDir temp_dir_;
};

TEST_F(IsolatedWebAppURLLoaderFactoryInstalledBundleTest, RequestIndex) {
  CreateFactory();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = kPrimaryUrl;
  EXPECT_THAT(CreateLoaderAndRun(std::move(request)), Eq(net::OK));
  ASSERT_THAT(ResponseInfo(), NotNull());
  EXPECT_THAT(ResponseInfo()->headers->response_code(), Eq(200));
  EXPECT_THAT(ResponseBody(), Eq("Hello World"));
}

TEST_F(IsolatedWebAppURLLoaderFactoryInstalledBundleTest,
       RequestResourceWithNon200StatusCode) {
  CreateFactory();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL(kPrimaryUrl.spec() + "/invalid-status-code");
  EXPECT_THAT(CreateLoaderAndRun(std::move(request)),
              Eq(net::ERR_INVALID_WEB_BUNDLE));
  EXPECT_THAT(ResponseInfo(), IsNull());
}

TEST_F(IsolatedWebAppURLLoaderFactoryInstalledBundleTest,
       RequestNonExistingResource) {
  CreateFactory();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL(kPrimaryUrl.spec() + "/non-existing");
  EXPECT_EQ(CreateLoaderAndRun(std::move(request)),
            net::ERR_HTTP_RESPONSE_CODE_FAILURE);
  ASSERT_THAT(ResponseInfo(), NotNull());
  EXPECT_THAT(ResponseInfo()->headers->response_code(), Eq(404));
  EXPECT_THAT(ResponseBody(), Eq(""));
}

class IsolatedWebAppURLLoaderFactoryFeatureFlagDisabledTest
    : public IsolatedWebAppURLLoaderFactoryInstalledBundleTest {
 public:
  IsolatedWebAppURLLoaderFactoryFeatureFlagDisabledTest()
      : IsolatedWebAppURLLoaderFactoryInstalledBundleTest(false) {}
};

TEST_F(IsolatedWebAppURLLoaderFactoryFeatureFlagDisabledTest,
       RequestFailsWhenFeatureIsDisabled) {
  CreateFactory();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = kPrimaryUrl;
  EXPECT_THAT(CreateLoaderAndRun(std::move(request)), Eq(net::ERR_FAILED));
  EXPECT_THAT(ResponseInfo(), IsNull());
}

}  // namespace
}  // namespace web_app
