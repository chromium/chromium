// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/renderer/renderer_url_loader_throttle.h"

#include <string_view>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/safe_browsing/content/common/safe_browsing.mojom.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_url_checker.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/http_request_headers_update_params.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class MockThrottleDelegate : public blink::URLLoaderThrottle::Delegate {
 public:
  ~MockThrottleDelegate() override = default;

  void CancelWithError(int error_code,
                       std::string_view custom_reason) override {}
  void Resume() override {}
};

class SBRendererUrlLoaderThrottleTest : public ::testing::Test {
 protected:
  SBRendererUrlLoaderThrottleTest() {
    throttle_delegate_ = std::make_unique<MockThrottleDelegate>();
    throttle_ = std::make_unique<RendererURLLoaderThrottle>();
    throttle_->set_delegate(throttle_delegate_.get());
  }

  network::ResourceRequest GetResourceRequest(
      const GURL& url,
      network::mojom::RequestDestination destination) {
    network::ResourceRequest request;
    request.url = url;
    request.destination = destination;
    return request;
  }

  base::test::TaskEnvironment message_loop_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  // Must outlive `throttle_`.
  std::unique_ptr<MockThrottleDelegate> throttle_delegate_;
  std::unique_ptr<RendererURLLoaderThrottle> throttle_;
};

TEST_F(SBRendererUrlLoaderThrottleTest, DoesNotDeferHttpsImageUrl) {
  GURL url("https://example.com/");
  bool defer = false;
  network::ResourceRequest request =
      GetResourceRequest(url, network::mojom::RequestDestination::kImage);
  throttle_->WillStartRequest(&request, &defer);
  message_loop_.RunUntilIdle();

  auto response_head = network::mojom::URLResponseHead::New();
  throttle_->WillProcessResponse(url, response_head.get(), &defer);
  EXPECT_FALSE(defer);
}

TEST_F(SBRendererUrlLoaderThrottleTest, DoesNotDeferHttpsScriptUrl) {
  GURL url("https://example.com/");
  bool defer = false;
  network::ResourceRequest request =
      GetResourceRequest(url, network::mojom::RequestDestination::kScript);
  throttle_->WillStartRequest(&request, &defer);
  message_loop_.RunUntilIdle();

  auto response_head = network::mojom::URLResponseHead::New();
  throttle_->WillProcessResponse(url, response_head.get(), &defer);
  EXPECT_FALSE(defer);
}

TEST_F(SBRendererUrlLoaderThrottleTest, DoesNotDeferChromeUrl) {
  GURL url("chrome://settings/");
  bool defer = false;
  network::ResourceRequest request =
      GetResourceRequest(url, network::mojom::RequestDestination::kScript);
  throttle_->WillStartRequest(&request, &defer);

  auto response_head = network::mojom::URLResponseHead::New();
  throttle_->WillProcessResponse(url, response_head.get(), &defer);
  EXPECT_FALSE(defer);
}

TEST_F(SBRendererUrlLoaderThrottleTest, DoesNotDeferIframeUrl) {
  GURL url("https://example.com/");
  bool defer = false;
  network::ResourceRequest request =
      GetResourceRequest(url, network::mojom::RequestDestination::kIframe);
  throttle_->WillStartRequest(&request, &defer);
  message_loop_.RunUntilIdle();

  auto response_head = network::mojom::URLResponseHead::New();
  throttle_->WillProcessResponse(url, response_head.get(), &defer);
  EXPECT_FALSE(defer);
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
class MockExtensionWebRequestReporter
    : public mojom::ExtensionWebRequestReporter {
 public:
  MockExtensionWebRequestReporter() = default;
  ~MockExtensionWebRequestReporter() override = default;

  void SendWebRequestData(
      const std::string& extension_id,
      const GURL& url,
      mojom::WebRequestProtocolType protocol_type,
      mojom::WebRequestContactInitiatorType initiator_type) override {}

  void Clone(mojo::PendingReceiver<mojom::ExtensionWebRequestReporter> receiver)
      override {
    clone_receiver_.Bind(std::move(receiver));
  }

  mojo::Receiver<mojom::ExtensionWebRequestReporter> receiver_{this};

 private:
  mojo::Receiver<mojom::ExtensionWebRequestReporter> clone_receiver_{this};
};

TEST_F(SBRendererUrlLoaderThrottleTest,
       WillRedirectRequest_ProviderDestroyed_NoCrash) {
  auto reporter = std::make_unique<MockExtensionWebRequestReporter>();
  mojo::Remote<mojom::ExtensionWebRequestReporter> remote;
  reporter->receiver_.Bind(remote.BindNewPipeAndPassReceiver());

  mojo::PendingRemote<mojom::ExtensionWebRequestReporter> pending_remote;
  remote->Clone(pending_remote.InitWithNewPipeAndPassReceiver());

  throttle_ =
      std::make_unique<RendererURLLoaderThrottle>(std::move(pending_remote));
  throttle_->set_delegate(throttle_delegate_.get());

  GURL url("http://example.com/");
  bool defer = false;
  network::ResourceRequest request =
      GetResourceRequest(url, network::mojom::RequestDestination::kScript);
  request.request_initiator =
      url::Origin::Create(GURL("chrome-extension://abc"));
  throttle_->WillStartRequest(&request, &defer);
  EXPECT_FALSE(defer);

  net::RedirectInfo redirect_info;
  redirect_info.new_url = GURL("http://example.com/redirect");
  auto response_head = network::mojom::URLResponseHead::New();

  network::HttpRequestHeadersUpdateParams headers_update_params;
  throttle_->WillRedirectRequest(&redirect_info, *response_head, &defer,
                                 &headers_update_params);

  // Destroy the reporter, simulating the URLLoaderThrottleProviderImpl being
  // destroyed. This should cause the pipe in the throttle to be closed.
  base::RunLoop run_loop;
  remote.set_disconnect_handler(run_loop.QuitClosure());
  reporter.reset();

  // Run the message loop until the disconnection is detected. If the UAF bug
  // exists, this will crash.
  run_loop.Run();

  // If we reach here, the test has passed because it didn't crash.
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

}  // namespace safe_browsing
