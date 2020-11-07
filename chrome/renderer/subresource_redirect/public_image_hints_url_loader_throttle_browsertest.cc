// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/subresource_redirect/subresource_redirect_url_loader_throttle.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/renderer/subresource_redirect/public_image_hints_url_loader_throttle.h"
#include "chrome/renderer/subresource_redirect/subresource_redirect_hints_agent.h"
#include "chrome/renderer/subresource_redirect/subresource_redirect_util.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/previews_state.h"
#include "third_party/blink/public/platform/web_network_state_notifier.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_request.h"

namespace subresource_redirect {

int kRenderFrameID = 1;

namespace {

class SubresourceRedirectPublicImageHintsURLLoaderThrottleTest
    : public ChromeRenderViewTest {
 public:
  void DisableSubresourceRedirectFeature() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndDisableFeature(
        blink::features::kSubresourceRedirect);
  }

  void SetCompressPublicImagesHints(
      const std::vector<std::string>& public_image_urls) {
    subresource_redirect_hints_agent_->SetCompressPublicImagesHints(
        blink::mojom::CompressPublicImagesHints::New(public_image_urls));
  }

  std::unique_ptr<PublicImageHintsURLLoaderThrottle>
  CreatePublicImageHintsURLLoaderThrottle(
      const GURL& url,
      network::mojom::RequestDestination request_destination,
      int previews_state) {
    blink::WebURLRequest request;
    request.SetUrl(url);
    request.SetPreviewsState(previews_state);
    request.SetRequestDestination(request_destination);
    DCHECK(SubresourceRedirectURLLoaderThrottle::MaybeCreateThrottle(
               request, view_->GetMainRenderFrame()->GetRoutingID())
               .get() != nullptr);

    return std::make_unique<PublicImageHintsURLLoaderThrottle>(
        view_->GetMainRenderFrame()->GetRoutingID(),
        previews_state & blink::PreviewsTypes::SUBRESOURCE_REDIRECT_ON);
  }

 protected:
  void SetUp() override {
    ChromeRenderViewTest::SetUp();
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kSubresourceRedirect,
          {{"enable_subresource_server_redirect", "true"}}}},
        {});
    subresource_redirect_hints_agent_ =
        new SubresourceRedirectHintsAgent(view_->GetMainRenderFrame());
  }

 private:
  SubresourceRedirectHintsAgent* subresource_redirect_hints_agent_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(SubresourceRedirectPublicImageHintsURLLoaderThrottleTest,
       TestMaybeCreateThrottle) {
  struct TestCase {
    bool data_saver_enabled;
    bool is_subresource_redirect_feature_enabled;
    network::mojom::RequestDestination destination;
    int previews_state;
    const std::string url;
    bool expected_is_throttle_created;
  };

  const TestCase kTestCases[]{
      {true, true, network::mojom::RequestDestination::kImage,
       blink::PreviewsTypes::SUBRESOURCE_REDIRECT_ON,
       "https://www.test.com/test.jpg", true},

      // Failure cases
      {false, true, network::mojom::RequestDestination::kImage,
       blink::PreviewsTypes::SUBRESOURCE_REDIRECT_ON,
       "https://www.test.com/test.jpg", false},
      {true, false, network::mojom::RequestDestination::kImage,
       blink::PreviewsTypes::SUBRESOURCE_REDIRECT_ON,
       "https://www.test.com/test.jpg", false},
      {true, true, network::mojom::RequestDestination::kScript,
       blink::PreviewsTypes::SUBRESOURCE_REDIRECT_ON,
       "https://www.test.com/test.jpg", false},
      {true, true, network::mojom::RequestDestination::kImage,
       blink::PreviewsTypes::SUBRESOURCE_REDIRECT_ON,
       "http://www.test.com/test.jpg", false},
  };

  for (const TestCase& test_case : kTestCases) {
    blink::WebNetworkStateNotifier::SetSaveDataEnabled(
        test_case.data_saver_enabled);
    if (!test_case.is_subresource_redirect_feature_enabled) {
      DisableSubresourceRedirectFeature();
    }

    blink::WebURLRequest request;
    request.SetPreviewsState(test_case.previews_state);
    request.SetUrl(GURL(test_case.url));
    request.SetRequestDestination(test_case.destination);
    EXPECT_EQ(test_case.expected_is_throttle_created,
              SubresourceRedirectURLLoaderThrottle::MaybeCreateThrottle(
                  request, kRenderFrameID) != nullptr);
  }
}

TEST_F(SubresourceRedirectPublicImageHintsURLLoaderThrottleTest,
       TestGetSubresourceURL) {
  struct TestCase {
    int previews_state;
    GURL original_url;
    GURL redirected_subresource_url;  // Empty URL means there will be no
                                      // redirect.
  };

  const TestCase kTestCases[]{
      {
          blink::PreviewsTypes::SUBRESOURCE_REDIRECT_ON,
          GURL("https://www.test.com/public_img.jpg"),
          GetSubresourceURLForURL(GURL("https://www.test.com/public_img.jpg")),
      },
      {
          blink::PreviewsTypes::SUBRESOURCE_REDIRECT_ON,
          GURL("https://www.test.com/public_img.jpg#anchor"),
          GetSubresourceURLForURL(
              GURL("https://www.test.com/public_img.jpg#anchor")),
      },
      {
          blink::PreviewsTypes::SUBRESOURCE_REDIRECT_ON,
          GURL("https://www.test.com/"
               "public_img.jpg?public_arg1=bar&public_arg2"),
          GetSubresourceURLForURL(
              GURL("https://www.test.com/"
                   "public_img.jpg?public_arg1=bar&public_arg2")),
      },
      // Private images will not be redirected.
      {
          blink::PreviewsTypes::SUBRESOURCE_REDIRECT_ON,
          GURL("https://www.test.com/private_img.jpg"),
          GURL(),
      },
      {
          blink::PreviewsTypes::SUBRESOURCE_REDIRECT_ON,
          GURL("https://www.test.com/public_img.jpg&private_arg1=foo"),
          GURL(),
      },
  };
  blink::WebNetworkStateNotifier::SetSaveDataEnabled(true);

  SetCompressPublicImagesHints(
      {"https://www.test.com/public_img.jpg",
       "https://www.test.com/public_img.jpg#anchor",
       "https://www.test.com/public_img.jpg?public_arg1=bar&public_arg2"});

  for (const TestCase& test_case : kTestCases) {
    auto throttle = CreatePublicImageHintsURLLoaderThrottle(
        test_case.original_url, network::mojom::RequestDestination::kImage,
        test_case.previews_state);
    network::ResourceRequest request;
    request.url = test_case.original_url;
    request.destination = network::mojom::RequestDestination::kImage;
    request.previews_state = test_case.previews_state;
    bool defer = true;
    throttle->WillStartRequest(&request, &defer);

    EXPECT_EQ(defer, test_case.redirected_subresource_url.is_empty());
    if (!test_case.redirected_subresource_url.is_empty()) {
      EXPECT_EQ(request.url, test_case.redirected_subresource_url);
    } else {
      EXPECT_EQ(request.url, test_case.original_url);
    }
  }
}

TEST_F(SubresourceRedirectPublicImageHintsURLLoaderThrottleTest,
       DeferOverridenToFalse) {
  blink::WebNetworkStateNotifier::SetSaveDataEnabled(true);

  SetCompressPublicImagesHints({"https://www.test.com/test.jpg"});

  auto throttle = CreatePublicImageHintsURLLoaderThrottle(
      GURL("https://www.test.com/test.jpg"),
      network::mojom::RequestDestination::kImage,
      blink::PreviewsTypes::SUBRESOURCE_REDIRECT_ON);
  network::ResourceRequest request;
  request.url = GURL("https://www.test.com/test.jpg");
  request.destination = network::mojom::RequestDestination::kImage;
  request.previews_state = blink::PreviewsTypes::SUBRESOURCE_REDIRECT_ON;
  bool defer = true;

  throttle->WillStartRequest(&request, &defer);
  EXPECT_FALSE(defer);
}

}  // namespace
}  // namespace subresource_redirect
