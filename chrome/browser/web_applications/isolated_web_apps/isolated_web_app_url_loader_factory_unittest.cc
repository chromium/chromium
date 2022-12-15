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
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_trust_checker.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/pending_install_info.h"
#include "chrome/browser/web_applications/isolation_data.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/url_constants.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/test_support/signed_web_bundles/web_bundle_signer.h"
#include "components/web_package/web_bundle_builder.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/simple_url_loader_test_helper.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace web_app {
namespace {

using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::IsFalse;
using ::testing::IsNull;
using ::testing::IsTrue;
using ::testing::NotNull;

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

  absl::optional<network::ResourceRequest> request() const { return request_; }

  absl::optional<GURL> intercepted_url() const {
    if (request_.has_value()) {
      return request_->url;
    }
    return absl::nullopt;
  }

 private:
  bool Intercept(content::URLLoaderInterceptor::RequestParams* params) {
    request_ = params->url_request;
    content::URLLoaderInterceptor::WriteResponse(
        "HTTP/1.1 200 OK\n", "test body", params->client.get());
    return true;
  }

  content::URLLoaderInterceptor interceptor_;
  absl::optional<network::ResourceRequest> request_;
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
      CreateStoragePartitionForUrl(web_app->scope());
    }

    provider()->GetRegistrarMutable().registry().emplace(web_app->app_id(),
                                                         std::move(web_app));
  }

  void CreateFactory() {
    factory_.Bind(IsolatedWebAppURLLoaderFactory::Create(
        web_contents()->GetPrimaryMainFrame()->GetFrameTreeNodeId(),
        profile()));
  }

  void CreateFactoryForServiceWorker() {
    factory_.Bind(
        IsolatedWebAppURLLoaderFactory::CreateForServiceWorker(profile()));
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

  void CreateStoragePartitionForUrl(const GURL& url) {
    base::expected<IsolatedWebAppUrlInfo, std::string> url_info =
        IsolatedWebAppUrlInfo::Create(url);
    if (!url_info.has_value()) {
      CHECK(false) << "Can't  create url info for url " << url
                   << ", error: " << url_info.error();
    }

    content::StoragePartition* current_storage_partition =
        profile()->GetStoragePartition(
            url_info->storage_partition_config(profile()),
            /*can_create=*/false);

    CHECK(current_storage_partition == nullptr)
        << "Partition already exists for url: " << url;

    content::StoragePartition* new_storage_partition =
        profile()->GetStoragePartition(
            url_info->storage_partition_config(profile()),
            /*can_create=*/true);

    CHECK(new_storage_partition != nullptr);
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

  const std::string kDevWebBundleId =
      "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaac";
  const GURL kDevAppOriginUrl = GURL("isolated-app://" + kDevWebBundleId);
  const GURL kDevAppStartUrl = kDevAppOriginUrl.Resolve("/ix.html");
  const url::Origin kProxyOrigin =
      url::Origin::Create(GURL("https://proxy.example.com"));

 private:
  bool enable_isolated_web_apps_feature_flag_;
  base::test::ScopedFeatureList scoped_feature_list_;

  raw_ptr<FakeWebAppProvider> provider_;
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
  request->url = kDevAppStartUrl;
  EXPECT_THAT(CreateLoaderAndRun(std::move(request)),
              IsNetError(net::ERR_FAILED));
  EXPECT_THAT(ResponseInfo(), IsNull());
}

TEST_F(IsolatedWebAppURLLoaderFactoryTest,
       RequestFailsWithErrFailedIfInstalledAppNotIwa) {
  RegisterWebApp(CreateWebApp(kDevAppStartUrl));

  // Verify that a PWA is installed at kAppStartUrl's origin.
  absl::optional<AppId> installed_app =
      provider()->registrar_unsafe().FindInstalledAppWithUrlInScope(
          kDevAppStartUrl);
  EXPECT_THAT(installed_app.has_value(), IsTrue());

  CreateFactory();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = kDevAppStartUrl;
  EXPECT_THAT(CreateLoaderAndRun(std::move(request)),
              IsNetError(net::ERR_FAILED));
  EXPECT_THAT(ResponseInfo(), IsNull());
}

TEST_F(IsolatedWebAppURLLoaderFactoryTest,
       RequestFailsWithErrFailedIfAppNotLocallyInstalled) {
  std::unique_ptr<WebApp> iwa = CreateIsolatedWebApp(
      kDevAppStartUrl,
      IsolationData{IsolationData::DevModeProxy{.proxy_url = kProxyOrigin}});
  iwa->SetIsLocallyInstalled(false);
  RegisterWebApp(std::move(iwa));

  // Verify that a PWA is installed at kAppStartUrl's origin.
  absl::optional<AppId> installed_app =
      provider()->registrar_unsafe().FindAppWithUrlInScope(kDevAppStartUrl);
  EXPECT_THAT(installed_app.has_value(), IsTrue());

  CreateFactory();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = kDevAppStartUrl;
  EXPECT_THAT(CreateLoaderAndRun(std::move(request)),
              IsNetError(net::ERR_FAILED));
  EXPECT_THAT(ResponseInfo(), IsNull());
}

TEST_F(IsolatedWebAppURLLoaderFactoryTest, GetRequestsSucceed) {
  RegisterWebApp(CreateIsolatedWebApp(
      kDevAppStartUrl,
      IsolationData{IsolationData::DevModeProxy{.proxy_url = kProxyOrigin}}));

  CreateFactory();

  auto request = std::make_unique<network::ResourceRequest>();
  request->method = net::HttpRequestHeaders::kGetMethod;
  request->url = kDevAppStartUrl;
  int status = CreateLoaderAndRun(std::move(request));

  EXPECT_THAT(status, IsNetError(net::OK));
}

TEST_F(IsolatedWebAppURLLoaderFactoryTest, HeadRequestsSucceed) {
  RegisterWebApp(CreateIsolatedWebApp(
      kDevAppStartUrl,
      IsolationData{IsolationData::DevModeProxy{.proxy_url = kProxyOrigin}}));

  CreateFactory();

  auto request = std::make_unique<network::ResourceRequest>();
  request->method = net::HttpRequestHeaders::kHeadMethod;
  request->url = kDevAppStartUrl;
  int status = CreateLoaderAndRun(std::move(request));

  EXPECT_THAT(status, IsNetError(net::OK));
}

TEST_F(IsolatedWebAppURLLoaderFactoryTest,
       PostRequestsReturnMethodNotSupportedWhenAppIsInstalled) {
  RegisterWebApp(CreateIsolatedWebApp(
      kDevAppStartUrl,
      IsolationData{IsolationData::DevModeProxy{.proxy_url = kProxyOrigin}}));

  CreateFactory();

  auto request = std::make_unique<network::ResourceRequest>();
  request->method = net::HttpRequestHeaders::kPostMethod;
  request->url = kDevAppStartUrl;
  int status = CreateLoaderAndRun(std::move(request));

  EXPECT_THAT(status, IsNetError(net::OK));
  ASSERT_THAT(ResponseInfo(), NotNull());
  EXPECT_THAT(ResponseInfo()->headers->response_code(),
              IsHttpStatusCode(net::HTTP_METHOD_NOT_ALLOWED));
}

TEST_F(
    IsolatedWebAppURLLoaderFactoryTest,
    PostRequestsReturnMethodNotSupportedWhenAppIsInstalledAndThereIsPendingInstall) {
  RegisterWebApp(CreateIsolatedWebApp(
      kDevAppStartUrl, IsolationData{IsolationData::DevModeProxy{
                           .proxy_url = url::Origin::Create(
                               GURL("http://installed-app-proxy-url.com"))}}));

  IsolatedWebAppPendingInstallInfo::FromWebContents(*web_contents())
      .set_isolation_data(IsolationData{IsolationData::DevModeProxy{
          .proxy_url = url::Origin::Create(
              GURL("http://pending-install-proxy-url.com"))}});

  CreateFactory();

  auto request = std::make_unique<network::ResourceRequest>();
  const char* kUnsupportedHttpMethod = net::HttpRequestHeaders::kPostMethod;
  request->method = kUnsupportedHttpMethod;
  request->url = kDevAppStartUrl;
  int status = CreateLoaderAndRun(std::move(request));

  EXPECT_THAT(status, IsNetError(net::OK));
  ASSERT_THAT(ResponseInfo(), NotNull());
  EXPECT_THAT(ResponseInfo()->headers->response_code(),
              IsHttpStatusCode(net::HTTP_METHOD_NOT_ALLOWED));
}

TEST_F(IsolatedWebAppURLLoaderFactoryTest,
       RequestWithUnsupportedHttpMethodFailWithErrFailedIfAppNotInstalled) {
  CreateFactory();

  auto request = std::make_unique<network::ResourceRequest>();

  const char* kUnsupportedHttpMethod = net::HttpRequestHeaders::kPostMethod;
  request->method = kUnsupportedHttpMethod;
  request->url = kDevAppStartUrl;
  int status = CreateLoaderAndRun(std::move(request));

  EXPECT_THAT(status, IsNetError(net::ERR_FAILED));
  EXPECT_THAT(ResponseInfo(), IsNull());
}

TEST_F(IsolatedWebAppURLLoaderFactoryTest,
       RequestFailsWithErrFailedIfStoragePartitionDoesNotExist) {
  RegisterWebApp(CreateIsolatedWebApp(kDevAppStartUrl,
                                      IsolationData{IsolationData::DevModeProxy{
                                          .proxy_url = kProxyOrigin}}),
                 /*create_storage_partition=*/false);

  CreateFactory();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = kDevAppStartUrl;
  int status = CreateLoaderAndRun(std::move(request));

  EXPECT_THAT(status, IsNetError(net::ERR_FAILED));
}

TEST_F(IsolatedWebAppURLLoaderFactoryTest,
       RequestUsesNonDefaultStoragePartition) {
  RegisterWebApp(CreateIsolatedWebApp(
      kDevAppStartUrl,
      IsolationData{IsolationData::DevModeProxy{.proxy_url = kProxyOrigin}}));

  CreateFactory();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = kDevAppStartUrl;
  CreateLoaderAndRun(std::move(request));

  EXPECT_THAT(profile()->GetStoragePartitionCount(), Eq(2UL));
}

TEST_F(IsolatedWebAppURLLoaderFactoryTest,
       RequestSucceedsIfProxyUrlHasTrailingSlash) {
  RegisterWebApp(CreateIsolatedWebApp(
      kDevAppStartUrl,
      IsolationData{IsolationData::DevModeProxy{
          .proxy_url = url::Origin::Create(GURL("http://example.com/"))}}));

  CreateFactory();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = kDevAppStartUrl;
  int status = CreateLoaderAndRun(std::move(request));

  EXPECT_THAT(status, IsNetError(net::OK));
}

TEST_F(IsolatedWebAppURLLoaderFactoryTest,
       RequestSucceedsIfProxyUrlDoesNotHaveTrailingSlash) {
  RegisterWebApp(CreateIsolatedWebApp(
      kDevAppStartUrl,
      IsolationData{IsolationData::DevModeProxy{
          .proxy_url = url::Origin::Create(GURL("http://example.com"))}}));

  CreateFactory();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = kDevAppStartUrl;
  int status = CreateLoaderAndRun(std::move(request));

  EXPECT_THAT(status, IsNetError(net::OK));
}

TEST_F(IsolatedWebAppURLLoaderFactoryTest, ProxyUrlDoesNotHaveUrlQuery) {
  RegisterWebApp(CreateIsolatedWebApp(
      kDevAppStartUrl,
      IsolationData{IsolationData::DevModeProxy{
          .proxy_url = url::Origin::Create(GURL("http://example.com"))}}));

  CreateFactory();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL("isolated-app://" + kDevWebBundleId +
                      "?testingQueryToRemove=testValue");
  CreateLoaderAndRun(std::move(request));

  EXPECT_THAT(url_handler().intercepted_url(), Eq(GURL("http://example.com/")));
}

TEST_F(IsolatedWebAppURLLoaderFactoryTest, ProxyUrlDoesNotHaveUrlFragment) {
  RegisterWebApp(CreateIsolatedWebApp(
      kDevAppStartUrl,
      IsolationData{IsolationData::DevModeProxy{
          .proxy_url = url::Origin::Create(GURL("http://example.com"))}}));

  CreateFactory();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url =
      GURL("isolated-app://" + kDevWebBundleId + "#testFragmentToremove");
  CreateLoaderAndRun(std::move(request));

  EXPECT_THAT(url_handler().intercepted_url(), Eq(GURL("http://example.com/")));
}

TEST_F(IsolatedWebAppURLLoaderFactoryTest, ProxyUrlKeepsOriginUrlPath) {
  RegisterWebApp(CreateIsolatedWebApp(
      kDevAppStartUrl,
      IsolationData{IsolationData::DevModeProxy{
          .proxy_url = url::Origin::Create(GURL("http://example.com"))}}));

  CreateFactory();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL("isolated-app://" + kDevWebBundleId + "/foo/bar.html");
  CreateLoaderAndRun(std::move(request));

  EXPECT_THAT(url_handler().intercepted_url(),
              Eq(GURL("http://example.com/foo/bar.html")));
}

TEST_F(IsolatedWebAppURLLoaderFactoryTest, ProxyUrlRemovesOriginalRequestData) {
  RegisterWebApp(CreateIsolatedWebApp(
      kDevAppStartUrl,
      IsolationData{IsolationData::DevModeProxy{
          .proxy_url = url::Origin::Create(GURL("http://example.com"))}}));

  CreateFactory();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL("isolated-app://" + kDevWebBundleId + "/foo/bar.html");
  CreateLoaderAndRun(std::move(request));

  ASSERT_THAT(url_handler().intercepted_url(),
              Eq(GURL("http://example.com/foo/bar.html")));
  EXPECT_THAT(url_handler().request()->credentials_mode,
              Eq(network::mojom::CredentialsMode::kOmit));
  EXPECT_THAT(url_handler().request()->request_initiator, Eq(absl::nullopt));
}

TEST_F(IsolatedWebAppURLLoaderFactoryTest, ProxyRequestCopiesAcceptHeader) {
  RegisterWebApp(CreateIsolatedWebApp(
      kDevAppStartUrl,
      IsolationData{IsolationData::DevModeProxy{
          .proxy_url = url::Origin::Create(GURL("http://example.com"))}}));

  CreateFactory();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL("isolated-app://" + kDevWebBundleId + "/foo/bar.html");
  request->headers.SetHeader(net::HttpRequestHeaders::kAccept, "text/html");
  CreateLoaderAndRun(std::move(request));

  std::string accept_header_value;
  ASSERT_THAT(url_handler().request()->headers.GetHeader(
                  net::HttpRequestHeaders::kAccept, &accept_header_value),
              IsTrue());
  EXPECT_THAT(accept_header_value, Eq("text/html"));
}

TEST_F(IsolatedWebAppURLLoaderFactoryTest, ProxyRequestDisablesCaching) {
  RegisterWebApp(CreateIsolatedWebApp(
      kDevAppStartUrl,
      IsolationData{IsolationData::DevModeProxy{
          .proxy_url = url::Origin::Create(GURL("http://example.com"))}}));

  CreateFactory();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL("isolated-app://" + kDevWebBundleId + "/foo/bar.html");
  CreateLoaderAndRun(std::move(request));

  std::string cache_control_header_value;
  ASSERT_THAT(
      url_handler().request()->headers.GetHeader(
          net::HttpRequestHeaders::kCacheControl, &cache_control_header_value),
      IsTrue());
  EXPECT_THAT(cache_control_header_value, Eq("no-cache"));
}

TEST_F(IsolatedWebAppURLLoaderFactoryTest, ProxyRequestDefaultsToAcceptingAll) {
  RegisterWebApp(CreateIsolatedWebApp(
      kDevAppStartUrl,
      IsolationData{IsolationData::DevModeProxy{
          .proxy_url = url::Origin::Create(GURL("http://example.com"))}}));

  CreateFactory();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL("isolated-app://" + kDevWebBundleId + "/foo/bar.html");
  CreateLoaderAndRun(std::move(request));

  std::string accept_header_value;
  ASSERT_THAT(url_handler().request()->headers.GetHeader(
                  net::HttpRequestHeaders::kAccept, &accept_header_value),
              IsTrue());
  EXPECT_THAT(accept_header_value, Eq("*/*"));
}

TEST_F(IsolatedWebAppURLLoaderFactoryTest,
       DoNotReturnGeneratedPageWhenNotInstallingApplication) {
  RegisterWebApp(CreateIsolatedWebApp(
      kDevAppStartUrl,
      IsolationData{IsolationData::DevModeProxy{
          .proxy_url = url::Origin::Create(GURL("http://example.com"))}}));

  CreateFactory();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL("isolated-app://" + kDevWebBundleId +
                      "/.well-known/_generated_install_page.html");

  int status = CreateLoaderAndRun(std::move(request));

  EXPECT_THAT(status, IsNetError(net::OK));
  EXPECT_THAT(
      url_handler().intercepted_url(),
      Eq("http://example.com/.well-known/_generated_install_page.html"));
}

TEST_F(IsolatedWebAppURLLoaderFactoryTest,
       ReturnGeneratedPageWhenInstallingApplication) {
  IsolatedWebAppPendingInstallInfo::FromWebContents(*web_contents())
      .set_isolation_data(IsolationData{IsolationData::DevModeProxy{
          .proxy_url =
              url::Origin::Create(GURL("http://some-proxy-url.com"))}});

  RegisterWebApp(CreateIsolatedWebApp(
      kDevAppStartUrl,
      IsolationData{IsolationData::DevModeProxy{
          .proxy_url = url::Origin::Create(GURL("http://example.com"))}}));

  CreateFactory();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL("isolated-app://" + kDevWebBundleId +
                      "/.well-known/_generated_install_page.html");

  int status = CreateLoaderAndRun(std::move(request));

  EXPECT_THAT(status, IsNetError(net::OK));
  EXPECT_THAT(url_handler().intercepted_url(), Eq(absl::nullopt));
  ASSERT_THAT(ResponseInfo(), NotNull());
  EXPECT_THAT(ResponseInfo()->headers->response_code(), Eq(200));
  EXPECT_THAT(ResponseBody(), HasSubstr("/manifest.webmanifest"));
}

TEST_F(IsolatedWebAppURLLoaderFactoryTest,
       RequestsRedirectedToPendingInstallIsolationDataWhenAppIsInstalled) {
  IsolatedWebAppPendingInstallInfo::FromWebContents(*web_contents())
      .set_isolation_data(IsolationData{IsolationData::DevModeProxy{
          .proxy_url =
              url::Origin::Create(GURL("http://some-proxy-url.com"))}});

  RegisterWebApp(CreateIsolatedWebApp(
      kDevAppStartUrl,
      IsolationData{IsolationData::DevModeProxy{
          .proxy_url = url::Origin::Create(GURL("http://example.com"))}}));

  CreateFactory();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL("isolated-app://" + kDevWebBundleId +
                      "/some-resource-for-testing.html");

  int status = CreateLoaderAndRun(std::move(request));

  EXPECT_THAT(status, IsNetError(net::OK));
  EXPECT_THAT(
      url_handler().intercepted_url(),
      Eq(GURL("http://some-proxy-url.com/some-resource-for-testing.html")));
}

TEST_F(IsolatedWebAppURLLoaderFactoryTest,
       RequestsRedirectedToPendingInstallIsolationDataWhenAppIsNotInstalled) {
  CreateStoragePartitionForUrl(GURL("isolated-app://" + kDevWebBundleId));

  IsolatedWebAppPendingInstallInfo::FromWebContents(*web_contents())
      .set_isolation_data(IsolationData{IsolationData::DevModeProxy{
          .proxy_url =
              url::Origin::Create(GURL("http://some-proxy-url.com"))}});

  CreateFactory();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL("isolated-app://" + kDevWebBundleId +
                      "/some-resource-for-testing.html");

  int status = CreateLoaderAndRun(std::move(request));

  EXPECT_THAT(status, IsNetError(net::OK));
  EXPECT_THAT(
      url_handler().intercepted_url(),
      Eq(GURL("http://some-proxy-url.com/some-resource-for-testing.html")));
}

TEST_F(IsolatedWebAppURLLoaderFactoryTest,
       GeneratedInstallPageIsNotReturnedForNonInstallingApp) {
  RegisterWebApp(CreateWebApp(kDevAppStartUrl));

  CreateFactory();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL("isolated-app://" + kDevWebBundleId +
                      "/.well-known/_generated_install_page.html");
  int status = CreateLoaderAndRun(std::move(request));

  EXPECT_THAT(status, IsNetError(net::ERR_FAILED));
  EXPECT_THAT(url_handler().intercepted_url(), Eq(absl::nullopt));
  EXPECT_THAT(ResponseInfo(), IsNull());
}

class IsolatedWebAppURLLoaderFactorySignedWebBundleTest
    : public IsolatedWebAppURLLoaderFactoryTest,
      public ::testing::WithParamInterface<std::tuple</*is_dev_mode=*/bool,
                                                      /*relative_urls=*/bool>> {
 public:
  explicit IsolatedWebAppURLLoaderFactorySignedWebBundleTest(
      bool enable_isolated_web_apps_feature_flag = true)
      : IsolatedWebAppURLLoaderFactoryTest(
            enable_isolated_web_apps_feature_flag) {}

 protected:
  void SetUp() override {
    bool is_dev_mode = std::get<0>(GetParam());

    IsolatedWebAppURLLoaderFactoryTest::SetUp();

    EXPECT_THAT(temp_dir_.CreateUniqueTempDir(), IsTrue());

    auto bundle_path = CreateSignedBundleAndWriteToDisk();
    std::unique_ptr<WebApp> iwa = CreateIsolatedWebApp(
        kEd25519AppOriginUrl,
        is_dev_mode
            ? IsolationData{IsolationData::DevModeBundle{.path = bundle_path}}
            : IsolationData{
                  IsolationData::InstalledBundle{.path = bundle_path}});
    RegisterWebApp(std::move(iwa));
  }

  void TearDown() override {
    SetTrustedWebBundleIdsForTesting({});
    IsolatedWebAppURLLoaderFactoryTest::TearDown();
  }

  base::FilePath CreateSignedBundleAndWriteToDisk() {
    bool relative_urls = std::get<1>(GetParam());
    std::string base_url = relative_urls ? "/" : kEd25519AppOriginUrl.spec();

    web_package::WebBundleBuilder builder;
    builder.AddExchange(base_url,
                        {{":status", "200"}, {"content-type", "text/html"}},
                        "Hello World");
    builder.AddExchange(base_url + "invalid-status-code",
                        {{":status", "201"}, {"content-type", "text/html"}},
                        "Hello World");

    web_package::WebBundleSigner::KeyPair key_pair(kTestPublicKey,
                                                   kTestPrivateKey);
    return SignAndWriteBundleToDisk(builder.CreateBundle(), profile(),
                                    key_pair);
  }

  base::FilePath SignAndWriteBundleToDisk(
      const std::vector<uint8_t>& unsigned_bundle,
      Profile* profile,
      web_package::WebBundleSigner::KeyPair key_pair) {
    auto signed_bundle =
        web_package::WebBundleSigner::SignBundle(unsigned_bundle, {key_pair});

    base::FilePath web_bundle_path;
    CHECK(CreateTemporaryFileInDir(temp_dir_.GetPath(), &web_bundle_path));

    CHECK_EQ(static_cast<size_t>(base::WriteFile(
                 web_bundle_path, reinterpret_cast<char*>(signed_bundle.data()),
                 signed_bundle.size())),
             signed_bundle.size());

    return web_bundle_path;
  }

  void TrustWebBundleId() {
    SetTrustedWebBundleIdsForTesting(
        {*web_package::SignedWebBundleId::Create(kTestEd25519WebBundleId)});
  }

  const GURL kEd25519AppOriginUrl = GURL(
      base::StrCat({chrome::kIsolatedAppScheme, url::kStandardSchemeSeparator,
                    kTestEd25519WebBundleId}));

  base::ScopedTempDir temp_dir_;
};

TEST_P(IsolatedWebAppURLLoaderFactorySignedWebBundleTest, RequestIndex) {
  CreateFactory();
  TrustWebBundleId();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = kEd25519AppOriginUrl;
  EXPECT_THAT(CreateLoaderAndRun(std::move(request)), Eq(net::OK));
  ASSERT_THAT(ResponseInfo(), NotNull());
  EXPECT_THAT(ResponseInfo()->headers->response_code(), Eq(200));
  EXPECT_THAT(ResponseBody(), Eq("Hello World"));
}

TEST_P(IsolatedWebAppURLLoaderFactorySignedWebBundleTest,
       RequestIndexWithoutTrustedPublicKey) {
  CreateFactory();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = kEd25519AppOriginUrl;
  int status = CreateLoaderAndRun(std::move(request));

  // TODO(crbug.com/1365852): This should probably be `ERR_FAILED`, not
  // `ERR_INVALID_WEB_BUNDLE`.
  EXPECT_THAT(status, IsNetError(net::ERR_INVALID_WEB_BUNDLE));
  EXPECT_THAT(ResponseInfo(), IsNull());
}

TEST_P(IsolatedWebAppURLLoaderFactorySignedWebBundleTest,
       RequestResourceWithNon200StatusCode) {
  CreateFactory();
  TrustWebBundleId();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = kEd25519AppOriginUrl.Resolve("/invalid-status-code");
  EXPECT_THAT(CreateLoaderAndRun(std::move(request)),
              Eq(net::ERR_INVALID_WEB_BUNDLE));
  EXPECT_THAT(ResponseInfo(), IsNull());
}

TEST_P(IsolatedWebAppURLLoaderFactorySignedWebBundleTest,
       RequestNonExistingResource) {
  CreateFactory();
  TrustWebBundleId();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = kEd25519AppOriginUrl.Resolve("/non-existing");
  EXPECT_EQ(CreateLoaderAndRun(std::move(request)), net::OK);
  ASSERT_THAT(ResponseInfo(), NotNull());
  EXPECT_THAT(ResponseInfo()->headers->response_code(),
              IsHttpStatusCode(net::HTTP_NOT_FOUND));
  EXPECT_THAT(ResponseBody(), Eq(""));
}

TEST_P(IsolatedWebAppURLLoaderFactorySignedWebBundleTest,
       SuccessfulRequestHasCorrectLengthFields) {
  CreateFactory();
  TrustWebBundleId();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = kEd25519AppOriginUrl;
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

TEST_P(IsolatedWebAppURLLoaderFactorySignedWebBundleTest,
       NonExistingRequestHasCorrectLengthFields) {
  CreateFactory();
  TrustWebBundleId();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = kEd25519AppOriginUrl.Resolve("/non-existing");
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

INSTANTIATE_TEST_SUITE_P(
    All,
    IsolatedWebAppURLLoaderFactorySignedWebBundleTest,
    ::testing::Combine(::testing::Bool(), ::testing::Bool()),
    [](::testing::TestParamInfo<std::tuple<bool, bool>> param_info) {
      return base::StrCat(
          {std::get<0>(param_info.param) ? "DevModeBundle" : "InstalledBundle",
           std::get<1>(param_info.param) ? "RelativeUrls" : "AbsoluteUrls"});
    });

using IsolatedWebAppURLLoaderFactoryForServiceWorkerTest =
    IsolatedWebAppURLLoaderFactoryTest;

TEST_F(IsolatedWebAppURLLoaderFactoryForServiceWorkerTest, GetRequestsSucceed) {
  RegisterWebApp(CreateIsolatedWebApp(
      kDevAppStartUrl,
      IsolationData{IsolationData::DevModeProxy{.proxy_url = kProxyOrigin}}));

  CreateFactoryForServiceWorker();

  auto request = std::make_unique<network::ResourceRequest>();
  request->method = net::HttpRequestHeaders::kGetMethod;
  request->url = kDevAppStartUrl;
  int status = CreateLoaderAndRun(std::move(request));

  EXPECT_THAT(status, IsNetError(net::OK));
}

class IsolatedWebAppURLLoaderFactoryFeatureFlagDisabledTest
    : public IsolatedWebAppURLLoaderFactorySignedWebBundleTest {
 public:
  IsolatedWebAppURLLoaderFactoryFeatureFlagDisabledTest()
      : IsolatedWebAppURLLoaderFactorySignedWebBundleTest(false) {}
};

TEST_P(IsolatedWebAppURLLoaderFactoryFeatureFlagDisabledTest,
       RequestFailsWhenFeatureIsDisabled) {
  CreateFactory();
  TrustWebBundleId();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = kEd25519AppOriginUrl;
  EXPECT_THAT(CreateLoaderAndRun(std::move(request)), Eq(net::ERR_FAILED));
  EXPECT_THAT(ResponseInfo(), IsNull());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    IsolatedWebAppURLLoaderFactoryFeatureFlagDisabledTest,
    ::testing::Combine(::testing::Bool(), ::testing::Bool()),
    [](::testing::TestParamInfo<std::tuple<bool, bool>> param_info) {
      return base::StrCat(
          {std::get<0>(param_info.param) ? "DevModeBundle" : "InstalledBundle",
           std::get<1>(param_info.param) ? "RelativeUrls" : "AbsoluteUrls"});
    });

}  // namespace web_app
