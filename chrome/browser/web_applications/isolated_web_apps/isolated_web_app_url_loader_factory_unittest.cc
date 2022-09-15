// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_loader_factory.h"

#include <memory>
#include <string>

#include "chrome/browser/web_applications/isolation_data.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "content/public/test/simple_url_loader_test_helper.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
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
  return web_app;
}

class IsolatedWebAppURLLoaderFactoryTest : public WebAppTest {
 public:
  void SetUp() override {
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
  FakeWebAppProvider* provider_;

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

}  // namespace
}  // namespace web_app
