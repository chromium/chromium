// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_loader_factory.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
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
#include "content/public/browser/storage_partition_config.h"
#include "content/public/common/content_features.h"
#include "content/public/test/simple_url_loader_test_helper.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/base/net_errors.h"
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
using ::testing::HasSubstr;
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

MATCHER_P(IsNetError, err, net::ErrorToString(err)) {
  if (arg == err)
    return true;

  *result_listener << net::ErrorToString(arg);
  return false;
}

MATCHER_P(IsHttpStatusCode, err, net::GetHttpReasonPhrase(err)) {
  if (arg == err)
    return true;

  *result_listener << net::GetHttpReasonPhrase(
      static_cast<net::HttpStatusCode>(arg));
  return false;
}

std::unique_ptr<WebApp> CreateWebApp(const GURL& start_url) {
  AppId app_id = GenerateAppId(/*manifest_id=*/"", start_url);
  auto web_app = std::make_unique<WebApp>(app_id);
  web_app->SetStartUrl(start_url);
  web_app->SetScope(start_url.DeprecatedGetOriginAsURL());
  web_app->AddSource(WebAppManagement::Type::kCommandLine);
  web_app->SetIsLocallyInstalled(true);
  return web_app;
}

std::unique_ptr<WebApp> CreateIsolatedWebApp(const GURL& start_url,
                                             IsolationData isolation_data) {
  auto web_app = CreateWebApp(start_url);
  web_app->SetIsolationData(isolation_data);
  return web_app;
}

class ScopedUrlHandler {
 public:
  ScopedUrlHandler()
      : interceptor_(base::BindRepeating(&ScopedUrlHandler::Intercept,
                                         base::Unretained(this))) {}

  absl::optional<GURL> intercepted_url() const { return intercepted_url_; }

 private:
  bool Intercept(content::URLLoaderInterceptor::RequestParams* params) {
    intercepted_url_ = params->url_request.url;
    content::URLLoaderInterceptor::WriteResponse(
        "HTTP/1.1 200 OK\n", "test body", params->client.get());
    return true;
  }

  content::URLLoaderInterceptor interceptor_;
  absl::optional<GURL> intercepted_url_;
};

}  // namespace

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

    url_handler_ = std::make_unique<ScopedUrlHandler>();

    provider_ = FakeWebAppProvider::Get(profile());
    provider_->Start();
  }

  void TearDown() override {
    url_handler_.reset();

    WebAppTest::TearDown();
  }

 protected:
  FakeWebAppProvider* provider() { return provider_; }

  void RegisterWebApp(std::unique_ptr<WebApp> web_app,
                      bool create_storage_partition = true) {
    if (create_storage_partition) {
      auto url_info = IsolatedWebAppUrlInfo::Create(web_app->scope());
      profile()->GetStoragePartition(
          url_info->storage_partition_config(profile()),
          /*can_create=*/true);
    }

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
    loader->SetAllowHttpErrorResults(true);

    content::SimpleURLLoaderTestHelper helper;
    loader->DownloadToString(
        factory_.get(), helper.GetCallback(),
        network::SimpleURLLoader::kMaxBoundedStringDownloadSize);

    helper.WaitForCallback();
    completion_status_ = *loader->CompletionStatus();
    if (loader->ResponseInfo()) {
      response_info_ = loader->ResponseInfo()->Clone();
      response_body_ = *helper.response_body();

      int64_t body_length = response_body_.size();
      EXPECT_THAT(completion_status_.decoded_body_length, Eq(body_length));
    }
    return loader->NetError();
  }

  const ScopedUrlHandler& url_handler() {
    CHECK(url_handler_);
    return *url_handler_;
  }

  const network::URLLoaderCompletionStatus& CompletionStatus() {
    return completion_status_;
  }

  network::mojom::URLResponseHead* ResponseInfo() {
    return response_info_.get();
  }

  std::string ResponseBody() { return response_body_; }

  const std::string kWebBundleId =
      "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaac";
  const GURL kAppOriginUrl = GURL("isolated-app://" + kWebBundleId);
  const GURL kAppStartUrl = GURL(kAppOriginUrl.spec() + "/ix.html");
  const GURL kProxyUrl = GURL("https://proxy.example.com");

 private:
  bool enable_isolated_web_apps_feature_flag_;
  base::test::ScopedFeatureList scoped_feature_list_;

  FakeWebAppProvider* provider_;
  std::unique_ptr<ScopedUrlHandler> url_handler_;

  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  mojo::Remote<network::mojom::URLLoaderFactory> factory_;
  network::URLLoaderCompletionStatus completion_status_;
  network::mojom::URLResponseHeadPtr response_info_;
  std::string response_body_;
};

TEST_F(IsolatedWebAppURLLoaderFactoryTest,
       RequestFailsWithErrFailedIfAppNotInstalled) {
  CreateFactory();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = kAppStartUrl;
  EXPECT_THAT(CreateLoaderAndRun(std::move(request)),
              IsNetError(net::ERR_FAILED));
  EXPECT_THAT(ResponseInfo(), IsNull());
}

TEST_F(IsolatedWebAppURLLoaderFactoryTest,
       RequestFailsWithErrFailedIfInstalledAppNotIwa) {
  RegisterWebApp(CreateWebApp(kAppStartUrl));

  // Verify that a PWA is installed at kAppStartUrl's origin.
  absl::optional<AppId> installed_app =
      provider()->registrar().FindInstalledAppWithUrlInScope(kAppStartUrl);
  EXPECT_THAT(installed_app.has_value(), IsTrue());

  CreateFactory();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = kAppStartUrl;
  EXPECT_THAT(CreateLoaderAndRun(std::move(request)),
              IsNetError(net::ERR_FAILED));
  EXPECT_THAT(ResponseInfo(), IsNull());
}

TEST_F(IsolatedWebAppURLLoaderFactoryTest,
       RequestFailsWithErrFailedIfAppNotLocallyInstalled) {
  std::unique_ptr<WebApp> iwa = CreateIsolatedWebApp(
      kAppStartUrl, IsolationData{IsolationData::DevModeProxy{
                        .proxy_url = kProxyUrl.spec()}});
  iwa->SetIsLocallyInstalled(false);
  RegisterWebApp(std::move(iwa));

  // Verify that a PWA is installed at kAppStartUrl's origin.
  absl::optional<AppId> installed_app =
      provider()->registrar().FindAppWithUrlInScope(kAppStartUrl);
  EXPECT_THAT(installed_app.has_value(), IsTrue());

  CreateFactory();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = kAppStartUrl;
  EXPECT_THAT(CreateLoaderAndRun(std::move(request)),
              IsNetError(net::ERR_FAILED));
  EXPECT_THAT(ResponseInfo(), IsNull());
}

TEST_F(IsolatedWebAppURLLoaderFactoryTest, GetRequestsSucceed) {
  RegisterWebApp(CreateIsolatedWebApp(kAppStartUrl,
                                      IsolationData{IsolationData::DevModeProxy{
                                          .proxy_url = kProxyUrl.spec()}}));

  CreateFactory();

  auto request = std::make_unique<network::ResourceRequest>();
  request->method = net::HttpRequestHeaders::kGetMethod;
  request->url = kAppStartUrl;
  int status = CreateLoaderAndRun(std::move(request));

  EXPECT_THAT(status, IsNetError(net::OK));
}

TEST_F(IsolatedWebAppURLLoaderFactoryTest, HeadRequestsSucceed) {
  RegisterWebApp(CreateIsolatedWebApp(kAppStartUrl,
                                      IsolationData{IsolationData::DevModeProxy{
                                          .proxy_url = kProxyUrl.spec()}}));

  CreateFactory();

  auto request = std::make_unique<network::ResourceRequest>();
  request->method = net::HttpRequestHeaders::kHeadMethod;
  request->url = kAppStartUrl;
  int status = CreateLoaderAndRun(std::move(request));

  EXPECT_THAT(status, IsNetError(net::OK));
}

TEST_F(IsolatedWebAppURLLoaderFactoryTest,
       PostRequestsReturnMethodNotSupported) {
  RegisterWebApp(CreateIsolatedWebApp(kAppStartUrl,
                                      IsolationData{IsolationData::DevModeProxy{
                                          .proxy_url = kProxyUrl.spec()}}));

  CreateFactory();

  auto request = std::make_unique<network::ResourceRequest>();
  request->method = net::HttpRequestHeaders::kPostMethod;
  request->url = kAppStartUrl;
  int status = CreateLoaderAndRun(std::move(request));

  EXPECT_THAT(status, IsNetError(net::OK));
  ASSERT_THAT(ResponseInfo(), NotNull());
  EXPECT_THAT(ResponseInfo()->headers->response_code(),
              IsHttpStatusCode(net::HTTP_METHOD_NOT_ALLOWED));
}

TEST_F(IsolatedWebAppURLLoaderFactoryTest,
       PostRequestsFailWithErrFailedIfAppNotInstalled) {
  CreateFactory();

  auto request = std::make_unique<network::ResourceRequest>();
  request->method = net::HttpRequestHeaders::kPostMethod;
  request->url = kAppStartUrl;
  int status = CreateLoaderAndRun(std::move(request));

  EXPECT_THAT(status, IsNetError(net::ERR_FAILED));
  EXPECT_THAT(ResponseInfo(), IsNull());
}

TEST_F(IsolatedWebAppURLLoaderFactoryTest,
       RequestFailsWithErrFailedIfStoragePartitionDoesNotExist) {
  RegisterWebApp(CreateIsolatedWebApp(kAppStartUrl,
                                      IsolationData{IsolationData::DevModeProxy{
                                          .proxy_url = kProxyUrl.spec()}}),
                 /*create_storage_partition=*/false);

  CreateFactory();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = kAppStartUrl;
  int status = CreateLoaderAndRun(std::move(request));

  EXPECT_THAT(status, IsNetError(net::ERR_FAILED));
}

TEST_F(IsolatedWebAppURLLoaderFactoryTest,
       RequestUsesNonDefaultStoragePartition) {
  RegisterWebApp(CreateIsolatedWebApp(kAppStartUrl,
                                      IsolationData{IsolationData::DevModeProxy{
                                          .proxy_url = kProxyUrl.spec()}}));

  CreateFactory();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = kAppStartUrl;
  CreateLoaderAndRun(std::move(request));

  EXPECT_THAT(profile()->GetStoragePartitionCount(), Eq(2UL));
}

TEST_F(IsolatedWebAppURLLoaderFactoryTest, RequestFailsIfProxyUrlNotOrigin) {
  RegisterWebApp(CreateIsolatedWebApp(
      kAppStartUrl, IsolationData{IsolationData::DevModeProxy{
                        .proxy_url = "http://example.com/foo"}}));

  CreateFactory();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = kAppStartUrl;
  int status = CreateLoaderAndRun(std::move(request));

  EXPECT_THAT(status, IsNetError(net::ERR_FAILED));
}

TEST_F(IsolatedWebAppURLLoaderFactoryTest,
       RequestSucceedsIfProxyUrlHasTrailingSlash) {
  RegisterWebApp(CreateIsolatedWebApp(
      kAppStartUrl, IsolationData{IsolationData::DevModeProxy{
                        .proxy_url = "http://example.com/"}}));

  CreateFactory();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = kAppStartUrl;
  int status = CreateLoaderAndRun(std::move(request));

  EXPECT_THAT(status, IsNetError(net::OK));
}

TEST_F(IsolatedWebAppURLLoaderFactoryTest,
       RequestSucceedsIfProxyUrlDoesNotHaveTrailingSlash) {
  RegisterWebApp(CreateIsolatedWebApp(kAppStartUrl,
                                      IsolationData{IsolationData::DevModeProxy{
                                          .proxy_url = "http://example.com"}}));

  CreateFactory();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = kAppStartUrl;
  int status = CreateLoaderAndRun(std::move(request));

  EXPECT_THAT(status, IsNetError(net::OK));
}

TEST_F(IsolatedWebAppURLLoaderFactoryTest, ProxyUrlDoesNotHaveUrlQuery) {
  RegisterWebApp(CreateIsolatedWebApp(kAppStartUrl,
                                      IsolationData{IsolationData::DevModeProxy{
                                          .proxy_url = "http://example.com"}}));

  CreateFactory();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL("isolated-app://" + kWebBundleId +
                      "?testingQueryToRemove=testValue");
  CreateLoaderAndRun(std::move(request));

  EXPECT_THAT(url_handler().intercepted_url(), Eq(GURL("http://example.com/")));
}

TEST_F(IsolatedWebAppURLLoaderFactoryTest, ProxyUrlDoesNotHaveUrlFragment) {
  RegisterWebApp(CreateIsolatedWebApp(kAppStartUrl,
                                      IsolationData{IsolationData::DevModeProxy{
                                          .proxy_url = "http://example.com"}}));

  CreateFactory();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url =
      GURL("isolated-app://" + kWebBundleId + "#testFragmentToremove");
  CreateLoaderAndRun(std::move(request));

  EXPECT_THAT(url_handler().intercepted_url(), Eq(GURL("http://example.com/")));
}

TEST_F(IsolatedWebAppURLLoaderFactoryTest, ProxyUrlKeepsOriginUrlPath) {
  RegisterWebApp(CreateIsolatedWebApp(kAppStartUrl,
                                      IsolationData{IsolationData::DevModeProxy{
                                          .proxy_url = "http://example.com"}}));

  CreateFactory();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL("isolated-app://" + kWebBundleId + "/foo/bar.html");
  CreateLoaderAndRun(std::move(request));

  EXPECT_THAT(url_handler().intercepted_url(),
              Eq(GURL("http://example.com/foo/bar.html")));
}

TEST_F(IsolatedWebAppURLLoaderFactoryTest, GeneratedInstallPageIsReturned) {
  RegisterWebApp(CreateIsolatedWebApp(kAppStartUrl,
                                      IsolationData{IsolationData::DevModeProxy{
                                          .proxy_url = "http://example.com"}}));

  CreateFactory();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL("isolated-app://" + kWebBundleId +
                      "/.well-known/_generated_install_page.html");
  int status = CreateLoaderAndRun(std::move(request));

  EXPECT_THAT(status, IsNetError(net::OK));
  EXPECT_THAT(url_handler().intercepted_url(), Eq(absl::nullopt));
  ASSERT_THAT(ResponseInfo(), NotNull());
  EXPECT_THAT(ResponseInfo()->headers->response_code(), Eq(200));
  EXPECT_THAT(ResponseBody(), HasSubstr("/manifest.webmanifest"));
}

TEST_F(IsolatedWebAppURLLoaderFactoryTest,
       GeneratedInstallPageIsNotReturnedForNonIwa) {
  RegisterWebApp(CreateWebApp(kAppStartUrl));

  CreateFactory();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL("isolated-app://" + kWebBundleId +
                      "/.well-known/_generated_install_page.html");
  int status = CreateLoaderAndRun(std::move(request));

  EXPECT_THAT(status, IsNetError(net::ERR_FAILED));
  EXPECT_THAT(url_handler().intercepted_url(), Eq(absl::nullopt));
  EXPECT_THAT(ResponseInfo(), IsNull());
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
        kAppOriginUrl,
        IsolationData{IsolationData::InstalledBundle{.path = bundle_path}});
    RegisterWebApp(std::move(iwa));
  }

  base::FilePath CreateSignedBundleAndWriteToDisk() {
    web_package::WebBundleBuilder builder;
    builder.AddPrimaryURL(kAppOriginUrl.spec());
    builder.AddExchange(kAppOriginUrl.spec(),
                        {{":status", "200"}, {"content-type", "text/html"}},
                        "Hello World");
    builder.AddExchange(kAppOriginUrl.spec() + "/invalid-status-code",
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
  request->url = kAppOriginUrl;
  EXPECT_THAT(CreateLoaderAndRun(std::move(request)), Eq(net::OK));
  ASSERT_THAT(ResponseInfo(), NotNull());
  EXPECT_THAT(ResponseInfo()->headers->response_code(), Eq(200));
  EXPECT_THAT(ResponseBody(), Eq("Hello World"));
}

TEST_F(IsolatedWebAppURLLoaderFactoryInstalledBundleTest,
       RequestResourceWithNon200StatusCode) {
  CreateFactory();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL(kAppOriginUrl.spec() + "/invalid-status-code");
  EXPECT_THAT(CreateLoaderAndRun(std::move(request)),
              Eq(net::ERR_INVALID_WEB_BUNDLE));
  EXPECT_THAT(ResponseInfo(), IsNull());
}

TEST_F(IsolatedWebAppURLLoaderFactoryInstalledBundleTest,
       RequestNonExistingResource) {
  CreateFactory();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL(kAppOriginUrl.spec() + "/non-existing");
  EXPECT_EQ(CreateLoaderAndRun(std::move(request)), net::OK);
  ASSERT_THAT(ResponseInfo(), NotNull());
  EXPECT_THAT(ResponseInfo()->headers->response_code(),
              IsHttpStatusCode(net::HTTP_NOT_FOUND));
  EXPECT_THAT(ResponseBody(), Eq(""));
}

TEST_F(IsolatedWebAppURLLoaderFactoryInstalledBundleTest,
       SuccessfulRequestHasCorrectLengthFields) {
  CreateFactory();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = kAppOriginUrl;
  EXPECT_THAT(CreateLoaderAndRun(std::move(request)), Eq(net::OK));
  ASSERT_THAT(ResponseInfo(), NotNull());
  EXPECT_THAT(ResponseInfo()->headers->response_code(), Eq(200));

  int64_t body_length = ResponseBody().size();
  int64_t header_length =
      static_cast<int64_t>(ResponseInfo()->headers->raw_headers().size());
  EXPECT_THAT(CompletionStatus().encoded_data_length,
              Eq(body_length + header_length));
  EXPECT_THAT(CompletionStatus().encoded_body_length, Eq(body_length));
  EXPECT_THAT(CompletionStatus().decoded_body_length, Eq(body_length));
}

TEST_F(IsolatedWebAppURLLoaderFactoryInstalledBundleTest,
       NonExistingRequestHasCorrectLengthFields) {
  CreateFactory();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL(kAppOriginUrl.spec() + "/non-existing");
  EXPECT_THAT(CreateLoaderAndRun(std::move(request)), Eq(net::OK));
  ASSERT_THAT(ResponseInfo(), NotNull());
  EXPECT_THAT(ResponseInfo()->headers->response_code(),
              IsHttpStatusCode(net::HTTP_NOT_FOUND));

  int64_t body_length = ResponseBody().size();
  int64_t header_length =
      static_cast<int64_t>(ResponseInfo()->headers->raw_headers().size());
  EXPECT_THAT(CompletionStatus().encoded_data_length,
              Eq(body_length + header_length));
  EXPECT_THAT(CompletionStatus().encoded_body_length, Eq(body_length));
  EXPECT_THAT(CompletionStatus().decoded_body_length, Eq(body_length));
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
  request->url = kAppOriginUrl;
  EXPECT_THAT(CreateLoaderAndRun(std::move(request)), Eq(net::ERR_FAILED));
  EXPECT_THAT(ResponseInfo(), IsNull());
}

}  // namespace web_app
