// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_loader_factory.h"

#include <memory>
#include <string>

#include "chrome/browser/web_applications/test/web_app_test.h"
#include "content/public/common/content_features.h"
#include "content/public/test/simple_url_loader_test_helper.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {

using ::testing::Eq;
using ::testing::IsNull;

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
  }

 protected:
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
  const std::string kPrimaryUrl = "isolated-app://" + kWebBundleId;

 private:
  bool enable_isolated_web_apps_feature_flag_;
  base::test::ScopedFeatureList scoped_feature_list_;
  mojo::Remote<network::mojom::URLLoaderFactory> factory_;
  network::mojom::URLResponseHeadPtr response_info_;
  std::string response_body_;
};

TEST_F(IsolatedWebAppURLLoaderFactoryTest, NotImplemented) {
  CreateFactory();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL(kPrimaryUrl);
  EXPECT_THAT(CreateLoaderAndRun(std::move(request)), Eq(net::ERR_FAILED));
  EXPECT_THAT(ResponseInfo(), IsNull());
}

class IsolatedWebAppURLLoaderFactoryFeatureFlagDisabledTest
    : public IsolatedWebAppURLLoaderFactoryTest {
 public:
  IsolatedWebAppURLLoaderFactoryFeatureFlagDisabledTest()
      : IsolatedWebAppURLLoaderFactoryTest(false) {}
};

TEST_F(IsolatedWebAppURLLoaderFactoryFeatureFlagDisabledTest,
       RequestFailsWhenFeatureIsDisabled) {
  CreateFactory();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL(kPrimaryUrl);
  EXPECT_THAT(CreateLoaderAndRun(std::move(request)), Eq(net::ERR_FAILED));
  EXPECT_THAT(ResponseInfo(), IsNull());
}

}  // namespace web_app
