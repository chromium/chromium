// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_loader_factory.h"

#include <memory>
#include <optional>
#include <string>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_source.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_storage_location.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_trust_checker.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolation_data.h"
#include "chrome/browser/web_applications/isolated_web_apps/pending_install_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/test/fake_web_app_database_factory.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/test_support/signed_web_bundles/web_bundle_signer.h"
#include "components/web_package/web_bundle_builder.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/render_frame_host.h"
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
#include "services/network/public/cpp/parsed_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/parsed_headers.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace web_app {
namespace {

using ::testing::_;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::IsFalse;
using ::testing::IsNull;
using ::testing::IsTrue;
using ::testing::NotNull;

inline constexpr uint8_t kTestPublicKey[] = {
    0xE4, 0xD5, 0x16, 0xC9, 0x85, 0x9A, 0xF8, 0x63, 0x56, 0xA3, 0x51,
    0x66, 0x7D, 0xBD, 0x00, 0x43, 0x61, 0x10, 0x1A, 0x92, 0xD4, 0x02,
    0x72, 0xFE, 0x2B, 0xCE, 0x81, 0xBB, 0x3B, 0x71, 0x3F, 0x2D};

inline constexpr uint8_t kTestPrivateKey[] = {
    0x1F, 0x27, 0x3F, 0x93, 0xE9, 0x59, 0x4E, 0xC7, 0x88, 0x82, 0xC7, 0x49,
    0xF8, 0x79, 0x3D, 0x8C, 0xDB, 0xE4, 0x60, 0x1C, 0x21, 0xF1, 0xD9, 0xF9,
    0xBC, 0x3A, 0xB5, 0xC7, 0x7F, 0x2D, 0x95, 0xE1,
    // public key (part of the private key)
    0xE4, 0xD5, 0x16, 0xC9, 0x85, 0x9A, 0xF8, 0x63, 0x56, 0xA3, 0x51, 0x66,
    0x7D, 0xBD, 0x00, 0x43, 0x61, 0x10, 0x1A, 0x92, 0xD4, 0x02, 0x72, 0xFE,
    0x2B, 0xCE, 0x81, 0xBB, 0x3B, 0x71, 0x3F, 0x2D};

// Derived from `kTestPublicKey`.
inline constexpr std::string_view kTestEd25519WebBundleId =
    "4tkrnsmftl4ggvvdkfth3piainqragus2qbhf7rlz2a3wo3rh4wqaaic";

MATCHER_P(IsNetError, err, net::ErrorToString(err)) {
  if (arg == err) {
    return true;
  }

  *result_listener << net::ErrorToString(arg);
  return false;
}

MATCHER_P(IsHttpStatusCode, err, net::GetHttpReasonPhrase(err)) {
  if (arg == err) {
    return true;
  }

  *result_listener << net::GetHttpReasonPhrase(
      static_cast<net::HttpStatusCode>(arg));
  return false;
}

std::unique_ptr<WebApp> CreateWebApp(const GURL& start_url) {
  webapps::AppId app_id = GenerateAppId(/*manifest_id_path=*/"", start_url);
  auto web_app = std::make_unique<WebApp>(app_id);
  web_app->SetName("iwa name");
  web_app->SetStartUrl(start_url);
  web_app->SetScope(start_url.DeprecatedGetOriginAsURL());
  web_app->SetManifestId(start_url.DeprecatedGetOriginAsURL());
  web_app->AddSource(WebAppManagement::Type::kIwaUserInstalled);
  web_app->SetUserDisplayMode(mojom::UserDisplayMode::kStandalone);
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

  std::optional<network::ResourceRequest> request() const { return request_; }

  std::optional<GURL> intercepted_url() const {
    if (request_.has_value()) {
      return request_->url;
    }
    return std::nullopt;
  }

 private:
  bool Intercept(content::URLLoaderInterceptor::RequestParams* params) {
    request_ = params->url_request;
    content::URLLoaderInterceptor::WriteResponse(
        "HTTP/1.1 200 OK\n", "test body", params->client.get());
    return true;
  }

  content::URLLoaderInterceptor interceptor_;
  std::optional<network::ResourceRequest> request_;
};

}  // namespace

class IsolatedWebAppURLLoaderFactoryTestBase : public WebAppTest {
 public:
  explicit IsolatedWebAppURLLoaderFactoryTestBase(
      const base::flat_map<base::test::FeatureRef, bool>& feature_states = {
          {features::kIsolatedWebApps, true},
          {features::kIsolatedWebAppDevMode, true}}) {
    scoped_feature_list_.InitWithFeatureStates(feature_states);
  }

  void SetUp() override {
    WebAppTest::SetUp();

    url_handler_ = std::make_unique<ScopedUrlHandler>();
  }

  void TearDown() override {
    url_handler_.reset();

    WebAppTest::TearDown();
  }

 protected:
  void CreateStoragePartitionForUrl(const GURL& url) {
    base::expected<IsolatedWebAppUrlInfo, std::string> url_info =
        IsolatedWebAppUrlInfo::Create(url);
    CHECK(url_info.has_value()) << "Can't create url info for url " << url
                                << ", error: " << url_info.error();

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

  const std::string kDevWebBundleId =
      "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaac";
  const GURL kDevAppOriginUrl = GURL("isolated-app://" + kDevWebBundleId);
  const GURL kDevAppStartUrl = kDevAppOriginUrl.Resolve("/ix.html");
  const url::Origin kProxyOrigin =
      url::Origin::Create(GURL("http://proxy.example.com"));

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  std::unique_ptr<ScopedUrlHandler> url_handler_;
};

class IsolatedWebAppURLLoaderFactoryTest
    : public IsolatedWebAppURLLoaderFactoryTestBase {
 public:
  using IsolatedWebAppURLLoaderFactoryTestBase::
      IsolatedWebAppURLLoaderFactoryTestBase;

  void SetUp() override {
    IsolatedWebAppURLLoaderFactoryTestBase::SetUp();

    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

 protected:
  void RegisterWebApp(std::unique_ptr<WebApp> web_app,
                      bool create_storage_partition = true) {
    if (create_storage_partition) {
      CreateStoragePartitionForUrl(web_app->scope());
    }

    fake_provider().GetRegistrarMutable().registry().emplace(
        web_app->app_id(), std::move(web_app));
  }

  void CreateFactoryForFrame(
      std::optional<url::Origin> app_origin = std::nullopt) {
    factory_.Bind(IsolatedWebAppURLLoaderFactory::CreateForFrame(
        profile(), app_origin,
        web_contents()->GetPrimaryMainFrame()->GetFrameTreeNodeId()));
  }

  void CreateFactoryForWorker() {
    factory_.Bind(
        IsolatedWebAppURLLoaderFactory::Create(profile(),
                                               /*app_origin=*/std::nullopt));
  }

  void CreateFactoryForBrowser() {
    factory_.Bind(IsolatedWebAppURLLoaderFactory::Create(
        profile(), /*app_origin=*/std::nullopt));
  }

  int CreateLoaderAndRun(std::unique_ptr<network::ResourceRequest> request) {
    auto loader = network::SimpleURLLoader::Create(
        std::move(request), TRAFFIC_ANNOTATION_FOR_TESTS);
    loader->SetAllowHttpErrorResults(true);

    content::SimpleURLLoaderTestHelper helper;
    loader->DownloadToString(
        factory_.get(), helper.GetCallbackDeprecated(),
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

  const network::URLLoaderCompletionStatus& CompletionStatus() {
    return completion_status_;
  }

  network::mojom::URLResponseHead* ResponseInfo() {
    return response_info_.get();
  }

  std::string ResponseBody() { return response_body_; }

  std::string GetResponseHeader(std::string_view name) {
    std::string value;
    ResponseInfo()->headers->GetNormalizedHeader(name, &value);
    return value;
  }

  network::mojom::ParsedHeadersPtr ParseHeaders(const GURL& request_url) {
    return network::PopulateParsedHeaders(ResponseInfo()->headers.get(),
                                          request_url);
  }

 private:
  mojo::Remote<network::mojom::URLLoaderFactory> factory_;
  network::URLLoaderCompletionStatus completion_status_;
  network::mojom::URLResponseHeadPtr response_info_;
  std::string response_body_;
};

TEST_F(IsolatedWebAppURLLoaderFactoryTest,
       RequestFailsWithErrFailedIfAppNotInstalled) {
  CreateFactoryForFrame();

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
  std::optional<webapps::AppId> installed_app =
      fake_provider().registrar_unsafe().FindInstalledAppWithUrlInScope(
          kDevAppStartUrl);
  EXPECT_THAT(installed_app.has_value(), IsTrue());

  CreateFactoryForFrame();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = kDevAppStartUrl;
  EXPECT_THAT(CreateLoaderAndRun(std::move(request)),
              IsNetError(net::ERR_FAILED));
  EXPECT_THAT(ResponseInfo(), IsNull());
}

TEST_F(IsolatedWebAppURLLoaderFactoryTest,
       RequestFailsWithErrFailedIfAppNotLocallyInstalled) {
  std::unique_ptr<WebApp> iwa = CreateIsolatedWebApp(
      kDevAppStartUrl, IsolationData::Builder(IwaStorageProxy{kProxyOrigin},
                                              base::Version("1.0.0"))
                           .Build());
  iwa->SetInstallState(proto::InstallState::SUGGESTED_FROM_ANOTHER_DEVICE);
  RegisterWebApp(std::move(iwa));

  // Verify that a PWA is installed at kAppStartUrl's origin.
  std::optional<webapps::AppId> installed_app =
      fake_provider().registrar_unsafe().FindAppWithUrlInScope(kDevAppStartUrl);
  EXPECT_THAT(installed_app.has_value(), IsTrue());

  CreateFactoryForFrame();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = kDevAppStartUrl;
  EXPECT_THAT(CreateLoaderAndRun(std::move(request)),
              IsNetError(net::ERR_FAILED));
  EXPECT_THAT(ResponseInfo(), IsNull());
}

TEST_F(IsolatedWebAppURLLoaderFactoryTest, GetRequestsSucceed) {
  RegisterWebApp(CreateIsolatedWebApp(
      kDevAppStartUrl, IsolationData::Builder(IwaStorageProxy{kProxyOrigin},
                                              base::Version("1.0.0"))
                           .Build()));

  CreateFactoryForFrame();

  auto request = std::make_unique<network::ResourceRequest>();
  request->method = net::HttpRequestHeaders::kGetMethod;
  request->url = kDevAppStartUrl;
  int status = CreateLoaderAndRun(std::move(request));

  EXPECT_THAT(status, IsNetError(net::OK));
}

TEST_F(IsolatedWebAppURLLoaderFactoryTest, HeadRequestsSucceed) {
  RegisterWebApp(CreateIsolatedWebApp(
      kDevAppStartUrl, IsolationData::Builder(IwaStorageProxy{kProxyOrigin},
                                              base::Version("1.0.0"))
                           .Build()));

  CreateFactoryForFrame();

  auto request = std::make_unique<network::ResourceRequest>();
  request->method = net::HttpRequestHeaders::kHeadMethod;
  request->url = kDevAppStartUrl;
  int status = CreateLoaderAndRun(std::move(request));

  EXPECT_THAT(status, IsNetError(net::OK));
}

TEST_F(IsolatedWebAppURLLoaderFactoryTest,
       PostRequestsReturnMethodNotSupportedWhenAppIsInstalled) {
  RegisterWebApp(CreateIsolatedWebApp(
      kDevAppStartUrl, IsolationData::Builder(IwaStorageProxy{kProxyOrigin},
                                              base::Version("1.0.0"))
                           .Build()));

  CreateFactoryForFrame();

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
      kDevAppStartUrl,
      IsolationData::Builder(IwaStorageProxy{url::Origin::Create(
                                 GURL("http://installed-app-proxy-url.com"))},
                             base::Version("1.0.0"))
          .Build()));

  IsolatedWebAppPendingInstallInfo::FromWebContents(*web_contents())
      .set_source(IwaSourceProxy{
          url::Origin::Create(GURL("http://pending-install-proxy-url.com"))});

  CreateFactoryForFrame();

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
  CreateFactoryForFrame();

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
  RegisterWebApp(
      CreateIsolatedWebApp(kDevAppStartUrl,
                           IsolationData::Builder(IwaStorageProxy{kProxyOrigin},
                                                  base::Version("1.0.0"))
                               .Build()),
      /*create_storage_partition=*/false);

  CreateFactoryForFrame();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = kDevAppStartUrl;
  int status = CreateLoaderAndRun(std::move(request));

  EXPECT_THAT(status, IsNetError(net::ERR_FAILED));
}

TEST_F(IsolatedWebAppURLLoaderFactoryTest,
       RequestUsesNonDefaultStoragePartition) {
  RegisterWebApp(CreateIsolatedWebApp(
      kDevAppStartUrl, IsolationData::Builder(IwaStorageProxy{kProxyOrigin},
                                              base::Version("1.0.0"))
                           .Build()));

  CreateFactoryForFrame();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = kDevAppStartUrl;
  CreateLoaderAndRun(std::move(request));

  EXPECT_THAT(profile()->GetLoadedStoragePartitionCount(), Eq(2UL));
}

TEST_F(IsolatedWebAppURLLoaderFactoryTest,
       RequestSucceedsIfProxyUrlHasTrailingSlash) {
  RegisterWebApp(CreateIsolatedWebApp(
      kDevAppStartUrl,
      IsolationData::Builder(
          IwaStorageProxy{url::Origin::Create(GURL("http://example.com/"))},
          base::Version("1.0.0"))
          .Build()));

  CreateFactoryForFrame();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = kDevAppStartUrl;
  int status = CreateLoaderAndRun(std::move(request));

  EXPECT_THAT(status, IsNetError(net::OK));
}

TEST_F(IsolatedWebAppURLLoaderFactoryTest,
       RequestSucceedsIfProxyUrlDoesNotHaveTrailingSlash) {
  RegisterWebApp(CreateIsolatedWebApp(
      kDevAppStartUrl,
      IsolationData::Builder(
          IwaStorageProxy{url::Origin::Create(GURL("http://example.com"))},
          base::Version("1.0.0"))
          .Build()));

  CreateFactoryForFrame();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = kDevAppStartUrl;
  int status = CreateLoaderAndRun(std::move(request));

  EXPECT_THAT(status, IsNetError(net::OK));
}

TEST_F(IsolatedWebAppURLLoaderFactoryTest, ProxyUrlInheritsQuery) {
  RegisterWebApp(CreateIsolatedWebApp(
      kDevAppStartUrl,
      IsolationData::Builder(
          IwaStorageProxy{url::Origin::Create(GURL("http://example.com"))},
          base::Version("1.0.0"))
          .Build()));

  CreateFactoryForFrame();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url =
      GURL("isolated-app://" + kDevWebBundleId + "?testingQuery=testValue");
  CreateLoaderAndRun(std::move(request));

  EXPECT_THAT(url_handler().intercepted_url(),
              Eq(GURL("http://example.com/?testingQuery=testValue")));
}

TEST_F(IsolatedWebAppURLLoaderFactoryTest, ProxyUrlDoesNotHaveUrlFragment) {
  RegisterWebApp(CreateIsolatedWebApp(
      kDevAppStartUrl,
      IsolationData::Builder(
          IwaStorageProxy{url::Origin::Create(GURL("http://example.com"))},
          base::Version("1.0.0"))
          .Build()));

  CreateFactoryForFrame();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url =
      GURL("isolated-app://" + kDevWebBundleId + "#testFragmentToremove");
  CreateLoaderAndRun(std::move(request));

  EXPECT_THAT(url_handler().intercepted_url(), Eq(GURL("http://example.com/")));
}

TEST_F(IsolatedWebAppURLLoaderFactoryTest, ProxyUrlKeepsOriginUrlPath) {
  RegisterWebApp(CreateIsolatedWebApp(
      kDevAppStartUrl,
      IsolationData::Builder(
          IwaStorageProxy{url::Origin::Create(GURL("http://example.com"))},
          base::Version("1.0.0"))
          .Build()));

  CreateFactoryForFrame();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL("isolated-app://" + kDevWebBundleId + "/foo/bar.html");
  CreateLoaderAndRun(std::move(request));

  EXPECT_THAT(url_handler().intercepted_url(),
              Eq(GURL("http://example.com/foo/bar.html")));
}

TEST_F(IsolatedWebAppURLLoaderFactoryTest, ProxyUrlRemovesOriginalRequestData) {
  RegisterWebApp(CreateIsolatedWebApp(
      kDevAppStartUrl,
      IsolationData::Builder(
          IwaStorageProxy{url::Origin::Create(GURL("http://example.com"))},
          base::Version("1.0.0"))
          .Build()));

  CreateFactoryForFrame();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL("isolated-app://" + kDevWebBundleId + "/foo/bar.html");
  CreateLoaderAndRun(std::move(request));

  ASSERT_THAT(url_handler().intercepted_url(),
              Eq(GURL("http://example.com/foo/bar.html")));
  EXPECT_THAT(url_handler().request()->credentials_mode,
              Eq(network::mojom::CredentialsMode::kOmit));
  EXPECT_THAT(url_handler().request()->request_initiator, Eq(std::nullopt));
}

TEST_F(IsolatedWebAppURLLoaderFactoryTest, ProxyRequestCopiesAcceptHeader) {
  RegisterWebApp(CreateIsolatedWebApp(
      kDevAppStartUrl,
      IsolationData::Builder(
          IwaStorageProxy{url::Origin::Create(GURL("http://example.com"))},
          base::Version("1.0.0"))
          .Build()));

  CreateFactoryForFrame();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL("isolated-app://" + kDevWebBundleId + "/foo/bar.html");
  request->headers.SetHeader(net::HttpRequestHeaders::kAccept, "text/html");
  CreateLoaderAndRun(std::move(request));

  EXPECT_THAT(url_handler().request()->headers.GetHeader(
                  net::HttpRequestHeaders::kAccept),
              testing::Optional(std::string("text/html")));
}

TEST_F(IsolatedWebAppURLLoaderFactoryTest, ProxyRequestDisablesCaching) {
  RegisterWebApp(CreateIsolatedWebApp(
      kDevAppStartUrl,
      IsolationData::Builder(
          IwaStorageProxy{url::Origin::Create(GURL("http://example.com"))},
          base::Version("1.0.0"))
          .Build()));

  CreateFactoryForFrame();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL("isolated-app://" + kDevWebBundleId + "/foo/bar.html");
  CreateLoaderAndRun(std::move(request));

  EXPECT_THAT(url_handler().request()->headers.GetHeader(
                  net::HttpRequestHeaders::kCacheControl),
              testing::Optional(std::string("no-cache")));
}

TEST_F(IsolatedWebAppURLLoaderFactoryTest, ProxyRequestDefaultsToAcceptingAll) {
  RegisterWebApp(CreateIsolatedWebApp(
      kDevAppStartUrl,
      IsolationData::Builder(
          IwaStorageProxy{url::Origin::Create(GURL("http://example.com"))},
          base::Version("1.0.0"))
          .Build()));

  CreateFactoryForFrame();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL("isolated-app://" + kDevWebBundleId + "/foo/bar.html");
  CreateLoaderAndRun(std::move(request));

  EXPECT_THAT(url_handler().request()->headers.GetHeader(
                  net::HttpRequestHeaders::kAccept),
              testing::Optional(std::string("*/*")));
}

TEST_F(IsolatedWebAppURLLoaderFactoryTest,
       DoNotReturnGeneratedPageWhenNotInstallingApplication) {
  RegisterWebApp(CreateIsolatedWebApp(
      kDevAppStartUrl,
      IsolationData::Builder(
          IwaStorageProxy{url::Origin::Create(GURL("http://example.com"))},
          base::Version("1.0.0"))
          .Build()));

  CreateFactoryForFrame();

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
      .set_source(IwaSourceProxy{
          url::Origin::Create(GURL("http://some-proxy-url.com"))});
  RegisterWebApp(CreateIsolatedWebApp(
      kDevAppStartUrl,
      IsolationData::Builder(
          IwaStorageProxy{url::Origin::Create(GURL("http://example.com"))},
          base::Version("1.0.0"))
          .Build()));

  CreateFactoryForFrame();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL("isolated-app://" + kDevWebBundleId +
                      "/.well-known/_generated_install_page.html");

  int status = CreateLoaderAndRun(std::move(request));

  EXPECT_THAT(status, IsNetError(net::OK));
  EXPECT_THAT(url_handler().intercepted_url(), Eq(std::nullopt));
  ASSERT_THAT(ResponseInfo(), NotNull());
  EXPECT_THAT(ResponseInfo()->headers->response_code(), Eq(200));
  EXPECT_THAT(ResponseBody(), HasSubstr("/.well-known/manifest.webmanifest"));
}

TEST_F(IsolatedWebAppURLLoaderFactoryTest,
       RequestsRedirectedToPendingInstallIsolationDataWhenAppIsInstalled) {
  IsolatedWebAppPendingInstallInfo::FromWebContents(*web_contents())
      .set_source(IwaSourceProxy{
          url::Origin::Create(GURL("http://some-proxy-url.com"))});

  RegisterWebApp(CreateIsolatedWebApp(
      kDevAppStartUrl,
      IsolationData::Builder(
          IwaStorageProxy{url::Origin::Create(GURL("http://example.com"))},
          base::Version("1.0.0"))
          .Build()));

  CreateFactoryForFrame();

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
      .set_source(IwaSourceProxy{
          url::Origin::Create(GURL("http://some-proxy-url.com"))});

  CreateFactoryForFrame();

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

  CreateFactoryForFrame();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL("isolated-app://" + kDevWebBundleId +
                      "/.well-known/_generated_install_page.html");
  int status = CreateLoaderAndRun(std::move(request));

  EXPECT_THAT(status, IsNetError(net::ERR_FAILED));
  EXPECT_THAT(url_handler().intercepted_url(), Eq(std::nullopt));
  EXPECT_THAT(ResponseInfo(), IsNull());
}

TEST_F(IsolatedWebAppURLLoaderFactoryTest,
       CannotRequestResourceFromDifferentIwa) {
  GURL other_iwa_origin{
      "isolated-app://"
      "abcdeqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaac"};
  RegisterWebApp(CreateIsolatedWebApp(
      other_iwa_origin, IsolationData::Builder(IwaStorageProxy{kProxyOrigin},
                                               base::Version("1.0.0"))
                            .Build()));

  RegisterWebApp(CreateIsolatedWebApp(
      kDevAppStartUrl, IsolationData::Builder(IwaStorageProxy{kProxyOrigin},
                                              base::Version("1.0.0"))
                           .Build()));
  NavigateAndCommit(kDevAppStartUrl);

  CreateFactoryForFrame(url::Origin::Create(kDevAppStartUrl));

  // Request a resource from a different IWA.
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = other_iwa_origin;
  int status = CreateLoaderAndRun(std::move(request));

  EXPECT_THAT(status, IsNetError(net::ERR_BLOCKED_BY_CLIENT));
  EXPECT_THAT(url_handler().intercepted_url(), Eq(std::nullopt));
  EXPECT_THAT(ResponseInfo(), IsNull());
}

TEST_F(IsolatedWebAppURLLoaderFactoryTest,
       BrowserCanRequestIwaResourceFromNonApp) {
  NavigateAndCommit(GURL("https://example.com"));

  RegisterWebApp(CreateIsolatedWebApp(
      kDevAppStartUrl, IsolationData::Builder(IwaStorageProxy{kProxyOrigin},
                                              base::Version("1.0.0"))
                           .Build()));

  CreateFactoryForBrowser();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = kDevAppStartUrl;
  int status = CreateLoaderAndRun(std::move(request));

  EXPECT_THAT(status, IsNetError(net::OK));
}

using IsolatedWebAppURLLoaderFactoryWebAppProviderReadyTest =
    IsolatedWebAppURLLoaderFactoryTestBase;

TEST_F(IsolatedWebAppURLLoaderFactoryWebAppProviderReadyTest, Waits) {
  IsolationData isolation_data =
      IsolationData::Builder(IwaStorageProxy{kProxyOrigin},
                             base::Version("1.0.0"))
          .Build();
  ASSERT_OK_AND_ASSIGN(auto url_info,
                       IsolatedWebAppUrlInfo::Create(kDevAppStartUrl));

  // Seed the `WebAppProvider` with an IWA before it is started.
  EXPECT_THAT(fake_provider().is_registry_ready(), IsFalse());
  {
    std::unique_ptr<WebApp> iwa =
        CreateIsolatedWebApp(kDevAppStartUrl, isolation_data);
    CreateStoragePartitionForUrl(iwa->scope());

    Registry registry;
    registry.emplace(iwa->app_id(), std::move(iwa));
    auto& database_factory = static_cast<FakeWebAppDatabaseFactory&>(
        fake_provider().database_factory());
    database_factory.WriteRegistry(registry);
  }

  mojo::Remote<network::mojom::URLLoaderFactory> factory;
  factory.Bind(IsolatedWebAppURLLoaderFactory::CreateForFrame(
      profile(), /*app_origin=*/std::nullopt,
      web_contents()->GetPrimaryMainFrame()->GetFrameTreeNodeId()));

  auto request = std::make_unique<network::ResourceRequest>();
  request->method = net::HttpRequestHeaders::kGetMethod;
  request->url = kDevAppStartUrl;
  auto loader = network::SimpleURLLoader::Create(std::move(request),
                                                 TRAFFIC_ANNOTATION_FOR_TESTS);
  loader->SetAllowHttpErrorResults(true);

  content::SimpleURLLoaderTestHelper helper;
  loader->DownloadToString(
      factory.get(), helper.GetCallbackDeprecated(),
      network::SimpleURLLoader::kMaxBoundedStringDownloadSize);

  task_environment()->RunUntilIdle();
  EXPECT_THAT(fake_provider().is_registry_ready(), IsFalse());
  test::AwaitStartWebAppProviderAndSubsystems(profile());
  EXPECT_THAT(fake_provider().registrar_unsafe().GetAppById(url_info.app_id()),
              test::IwaIs("iwa name", isolation_data));
  helper.WaitForCallback();

  EXPECT_THAT(loader->NetError(), IsNetError(net::OK));
}

using IsolatedWebAppURLLoaderFactoryForServiceWorkerTest =
    IsolatedWebAppURLLoaderFactoryTest;

TEST_F(IsolatedWebAppURLLoaderFactoryForServiceWorkerTest, GetRequestsSucceed) {
  RegisterWebApp(CreateIsolatedWebApp(
      kDevAppStartUrl, IsolationData::Builder(IwaStorageProxy{kProxyOrigin},
                                              base::Version("1.0.0"))
                           .Build()));

  CreateFactoryForWorker();

  auto request = std::make_unique<network::ResourceRequest>();
  request->method = net::HttpRequestHeaders::kGetMethod;
  request->url = kDevAppStartUrl;
  int status = CreateLoaderAndRun(std::move(request));

  EXPECT_THAT(status, IsNetError(net::OK));
}

class IsolatedWebAppURLLoaderFactorySignedWebBundleTestBase
    : public IsolatedWebAppURLLoaderFactoryTest {
 public:
  IsolatedWebAppURLLoaderFactorySignedWebBundleTestBase(
      const base::flat_map<base::test::FeatureRef, bool>& feature_states,
      bool is_dev_mode_bundle,
      bool relative_urls)
      : IsolatedWebAppURLLoaderFactoryTest(feature_states),
        is_dev_mode_bundle_(is_dev_mode_bundle),
        relative_urls_(relative_urls) {}

 protected:
  void SetUp() override {
    IsolatedWebAppURLLoaderFactoryTest::SetUp();

    EXPECT_THAT(temp_dir_.CreateUniqueTempDir(), IsTrue());

    base::FilePath bundle_path;
    std::optional<IsolatedWebAppStorageLocation> location;
    if (is_dev_mode_bundle_) {
      ASSERT_TRUE(CreateTemporaryFileInDir(temp_dir_.GetPath(), &bundle_path));
      location = IwaStorageUnownedBundle{bundle_path};
    } else {
      IwaStorageOwnedBundle source{"some_folder", /*dev_mode=*/false};
      bundle_path = source.GetPath(profile()->GetPath());
      ASSERT_TRUE(base::CreateDirectory(bundle_path.DirName()));
      location = source;
    }
    ASSERT_NO_FATAL_FAILURE(CreateSignedBundleAndWriteToDisk(bundle_path));

    std::unique_ptr<WebApp> iwa = CreateIsolatedWebApp(
        kEd25519AppOriginUrl,
        IsolationData::Builder(*location, base::Version("1.0.0")).Build());
    RegisterWebApp(std::move(iwa));
  }

  void TearDown() override {
    SetTrustedWebBundleIdsForTesting({});
    IsolatedWebAppURLLoaderFactoryTest::TearDown();
  }

  void CreateSignedBundleAndWriteToDisk(base::FilePath web_bundle_path) {
    std::string base_url = relative_urls_ ? "/" : kEd25519AppOriginUrl.spec();

    web_package::test::Ed25519KeyPair key_pair(kTestPublicKey, kTestPrivateKey);

    bundle_ =
        IsolatedWebAppBuilder(ManifestBuilder().SetStartUrl(base_url))
            .RemoveResource("/")
            .AddHtml(base_url, "Hello World")
            .AddResource(base_url + "invalid-status-code", "HelloWorld",
                         {{"Content-Type", "text/html"}},
                         static_cast<net::HttpStatusCode>(201))
            .AddResource(base_url + "no_coi.html", "No COI",
                         {
                             {"Cross-Origin-Opener-Policy", "unsafe-none"},
                             {"Cross-Origin-Embedder-Policy", "unsafe-none"},
                             {"Cross-Origin-Resource-Policy", "cross-origin"},
                         })
            .AddResource(base_url + "csp.html", "CSP",
                         {{"Content-Security-Policy", "default-src 'none'"}})
            .BuildBundle(web_bundle_path, key_pair);
  }

  void TrustWebBundleId() {
    SetTrustedWebBundleIdsForTesting(
        {*web_package::SignedWebBundleId::Create(kTestEd25519WebBundleId)});
  }

  const GURL kEd25519AppOriginUrl = GURL(
      base::StrCat({chrome::kIsolatedAppScheme, url::kStandardSchemeSeparator,
                    kTestEd25519WebBundleId}));

  bool is_dev_mode_bundle_;
  bool relative_urls_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<BundledIsolatedWebApp> bundle_;
};

class IsolatedWebAppURLLoaderFactorySignedWebBundleTest
    : public IsolatedWebAppURLLoaderFactorySignedWebBundleTestBase,
      public ::testing::WithParamInterface<
          std::tuple</*is_dev_mode_bundle=*/bool,
                     /*relative_urls=*/bool>> {
 public:
  explicit IsolatedWebAppURLLoaderFactorySignedWebBundleTest(
      const base::flat_map<base::test::FeatureRef, bool>& feature_states =
          {{features::kIsolatedWebApps, true},
           {features::kIsolatedWebAppDevMode, std::get<0>(GetParam())}})
      : IsolatedWebAppURLLoaderFactorySignedWebBundleTestBase(
            feature_states,
            /*is_dev_mode_bundle=*/std::get<0>(GetParam()),
            /*relative_urls=*/std::get<1>(GetParam())) {}
};

TEST_P(IsolatedWebAppURLLoaderFactorySignedWebBundleTest, RequestIndex) {
  CreateFactoryForFrame();
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
  CreateFactoryForFrame();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = kEd25519AppOriginUrl;
  int status = CreateLoaderAndRun(std::move(request));

  if (is_dev_mode_bundle_) {
    // All public keys of dev mode bundles are trusted when
    // `features::kIsolatedWebAppDevMode` is enabled.
    EXPECT_THAT(base::FeatureList::IsEnabled(features::kIsolatedWebAppDevMode),
                IsTrue());
    EXPECT_THAT(status, IsNetError(net::OK));
    EXPECT_THAT(ResponseInfo(), NotNull());
  } else {
    // TODO(crbug.com/40239530): This should probably be `ERR_FAILED`, not
    // `ERR_INVALID_WEB_BUNDLE`.
    EXPECT_THAT(status, IsNetError(net::ERR_INVALID_WEB_BUNDLE));
    EXPECT_THAT(ResponseInfo(), IsNull());
  }
}

TEST_P(IsolatedWebAppURLLoaderFactorySignedWebBundleTest,
       RequestResourceWithNon200StatusCode) {
  CreateFactoryForFrame();
  TrustWebBundleId();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = kEd25519AppOriginUrl.Resolve("/invalid-status-code");
  EXPECT_THAT(CreateLoaderAndRun(std::move(request)),
              Eq(net::ERR_INVALID_WEB_BUNDLE));
  EXPECT_THAT(ResponseInfo(), IsNull());
}

TEST_P(IsolatedWebAppURLLoaderFactorySignedWebBundleTest,
       RequestNonExistingResource) {
  CreateFactoryForFrame();
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
  CreateFactoryForFrame();
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
  EXPECT_THAT(ResponseInfo()->content_length, Eq(body_length));
}

TEST_P(IsolatedWebAppURLLoaderFactorySignedWebBundleTest,
       NonExistingRequestHasCorrectLengthFields) {
  CreateFactoryForFrame();
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

TEST_P(IsolatedWebAppURLLoaderFactorySignedWebBundleTest,
       ExistingCoiOverridden) {
  CreateFactoryForFrame();
  TrustWebBundleId();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = kEd25519AppOriginUrl.Resolve("/no_coi.html");
  int status = CreateLoaderAndRun(std::move(request));

  EXPECT_THAT(status, IsNetError(net::OK));
  EXPECT_THAT(GetResponseHeader("Cross-Origin-Opener-Policy"),
              Eq("same-origin"));
  EXPECT_THAT(GetResponseHeader("Cross-Origin-Embedder-Policy"),
              Eq("require-corp"));
  EXPECT_THAT(GetResponseHeader("Cross-Origin-Resource-Policy"),
              Eq("same-origin"));
}

TEST_P(IsolatedWebAppURLLoaderFactorySignedWebBundleTest, ExistingCspKept) {
  CreateFactoryForFrame();
  TrustWebBundleId();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = kEd25519AppOriginUrl.Resolve("/csp.html");
  int status = CreateLoaderAndRun(std::move(request));

  EXPECT_THAT(status, IsNetError(net::OK));

  network::mojom::ParsedHeadersPtr parsed_headers =
      ParseHeaders(kEd25519AppOriginUrl);
  ASSERT_EQ(2UL, parsed_headers->content_security_policy.size());
  const auto& bundled_csp = parsed_headers->content_security_policy[0];
  ASSERT_EQ(1UL, bundled_csp->raw_directives.size());
  using Directive = network::mojom::CSPDirectiveName;
  EXPECT_THAT(bundled_csp->raw_directives[Directive::DefaultSrc], Eq("'none'"));

  const auto& injected_csp = parsed_headers->content_security_policy[1];
  EXPECT_EQ(12UL, injected_csp->raw_directives.size());
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

class IsolatedWebAppURLLoaderFactoryFeatureFlagDisabledTest
    : public IsolatedWebAppURLLoaderFactorySignedWebBundleTest {
 public:
  IsolatedWebAppURLLoaderFactoryFeatureFlagDisabledTest()
      : IsolatedWebAppURLLoaderFactorySignedWebBundleTest(
            {{features::kIsolatedWebApps, false},
             {features::kIsolatedWebAppDevMode, true}}) {}
};

TEST_P(IsolatedWebAppURLLoaderFactoryFeatureFlagDisabledTest,
       RequestFailsWhenFeatureIsDisabled) {
  CreateFactoryForFrame();
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

class IsolatedWebAppURLLoaderFactoryDevModeDisabledTest
    : public IsolatedWebAppURLLoaderFactorySignedWebBundleTest {
 public:
  IsolatedWebAppURLLoaderFactoryDevModeDisabledTest()
      : IsolatedWebAppURLLoaderFactorySignedWebBundleTest(
            {{features::kIsolatedWebApps, true},
             {features::kIsolatedWebAppDevMode, false}}) {}
};

TEST_P(IsolatedWebAppURLLoaderFactoryDevModeDisabledTest,
       DevModeBundleRequestFailsWhenDevModeIsDisabled) {
  CreateFactoryForFrame();
  TrustWebBundleId();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = kEd25519AppOriginUrl;

  int status = CreateLoaderAndRun(std::move(request));
  if (is_dev_mode_bundle_) {
    EXPECT_THAT(status, IsNetError(net::ERR_FAILED));
    EXPECT_THAT(ResponseInfo(), IsNull());
  } else {
    EXPECT_THAT(status, IsNetError(net::OK));
    EXPECT_THAT(ResponseInfo(), NotNull());
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    IsolatedWebAppURLLoaderFactoryDevModeDisabledTest,
    ::testing::Combine(::testing::Bool(), ::testing::Bool()),
    [](::testing::TestParamInfo<std::tuple<bool, bool>> param_info) {
      return base::StrCat(
          {std::get<0>(param_info.param) ? "DevModeBundle" : "InstalledBundle",
           std::get<1>(param_info.param) ? "RelativeUrls" : "AbsoluteUrls"});
    });

class IsolatedWebAppURLLoaderFactoryHeaderTest
    : public IsolatedWebAppURLLoaderFactorySignedWebBundleTestBase,
      public ::testing::WithParamInterface</*is_bundle=*/bool> {
 protected:
  IsolatedWebAppURLLoaderFactoryHeaderTest()
      : IsolatedWebAppURLLoaderFactorySignedWebBundleTestBase(
            {{features::kIsolatedWebApps, true},
             {features::kIsolatedWebAppDevMode, true}},
            /*is_dev_mode_bundle=*/false,
            /*relative_urls=*/true),
        is_bundle_(GetParam()) {}

  void SetUp() override {
    IsolatedWebAppURLLoaderFactorySignedWebBundleTestBase::SetUp();

    RegisterWebApp(CreateIsolatedWebApp(
        kDevAppStartUrl, IsolationData::Builder(IwaStorageProxy{kProxyOrigin},
                                                base::Version("1.0.0"))
                             .Build()));
  }

  bool is_bundle() { return is_bundle_; }

  GURL GetAppOriginUrl() {
    return is_bundle() ? kEd25519AppOriginUrl : kDevAppStartUrl;
  }

 private:
  bool is_bundle_;
};

TEST_P(IsolatedWebAppURLLoaderFactoryHeaderTest, CoiInjected) {
  CreateFactoryForFrame();
  TrustWebBundleId();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GetAppOriginUrl();
  int status = CreateLoaderAndRun(std::move(request));

  EXPECT_THAT(status, IsNetError(net::OK));
  EXPECT_THAT(GetResponseHeader("Cross-Origin-Opener-Policy"),
              Eq("same-origin"));
  EXPECT_THAT(GetResponseHeader("Cross-Origin-Embedder-Policy"),
              Eq("require-corp"));
  EXPECT_THAT(GetResponseHeader("Cross-Origin-Resource-Policy"),
              Eq("same-origin"));

  network::mojom::ParsedHeadersPtr parsed_headers =
      ParseHeaders(GetAppOriginUrl());
  EXPECT_THAT(parsed_headers->cross_origin_opener_policy.value,
              Eq(network::mojom::CrossOriginOpenerPolicyValue::kSameOrigin));
  EXPECT_THAT(parsed_headers->cross_origin_embedder_policy.value,
              Eq(network::mojom::CrossOriginEmbedderPolicyValue::kRequireCorp));
}

TEST_P(IsolatedWebAppURLLoaderFactoryHeaderTest, CspInjected) {
  CreateFactoryForFrame();
  TrustWebBundleId();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GetAppOriginUrl();
  int status = CreateLoaderAndRun(std::move(request));

  EXPECT_THAT(status, IsNetError(net::OK));

  network::mojom::ParsedHeadersPtr parsed_headers =
      ParseHeaders(GetAppOriginUrl());
  ASSERT_EQ(1UL, parsed_headers->content_security_policy.size());
  const auto& csp = parsed_headers->content_security_policy[0];
  ASSERT_EQ(12UL, csp->raw_directives.size());
  using Directive = network::mojom::CSPDirectiveName;
  EXPECT_THAT(csp->raw_directives[Directive::BaseURI], Eq("'none'"));
  EXPECT_THAT(csp->raw_directives[Directive::DefaultSrc], Eq("'self'"));
  EXPECT_THAT(csp->raw_directives[Directive::ObjectSrc], Eq("'none'"));
  EXPECT_THAT(csp->raw_directives[Directive::FrameSrc],
              Eq("'self' https: blob: data:"));
  EXPECT_THAT(csp->raw_directives[Directive::ScriptSrc],
              Eq("'self' 'wasm-unsafe-eval'"));
  EXPECT_THAT(csp->raw_directives[Directive::ImgSrc],
              Eq("'self' https: blob: data:"));
  EXPECT_THAT(csp->raw_directives[Directive::MediaSrc],
              Eq("'self' https: blob: data:"));
  EXPECT_THAT(csp->raw_directives[Directive::FontSrc],
              Eq("'self' blob: data:"));
  EXPECT_THAT(csp->raw_directives[Directive::StyleSrc],
              Eq("'self' 'unsafe-inline'"));
  EXPECT_THAT(csp->raw_directives[Directive::RequireTrustedTypesFor],
              Eq("'script'"));
  EXPECT_THAT(csp->raw_directives[Directive::FrameAncestors], Eq("'self'"));
  if (is_bundle()) {
    EXPECT_THAT(csp->raw_directives[Directive::ConnectSrc],
                Eq("'self' https: wss: blob: data:"));
  } else {
    EXPECT_THAT(csp->raw_directives[Directive::ConnectSrc],
                Eq("'self' https: wss: blob: data: ws://proxy.example.com:80"));
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         IsolatedWebAppURLLoaderFactoryHeaderTest,
                         ::testing::Bool(),
                         [](::testing::TestParamInfo<bool> param_info) {
                           return param_info.param ? "Bundle" : "Proxy";
                         });

}  // namespace web_app
