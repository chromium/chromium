// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/renderer/renderer_url_loader_throttle.h"

#include "base/test/task_environment.h"
#include "components/safe_browsing/content/common/safe_browsing.mojom.h"
#include "components/safe_browsing/core/common/safe_browsing_url_checker.mojom.h"
#include "ipc/ipc_message.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/resource_request.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class FakeSafeBrowsing : public mojom::SafeBrowsing {
 public:
  FakeSafeBrowsing() = default;

  void CreateCheckerAndCheck(
      int32_t render_frame_id,
      mojo::PendingReceiver<mojom::SafeBrowsingUrlChecker> receiver,
      const GURL& url,
      const std::string& method,
      const net::HttpRequestHeaders& headers,
      int32_t load_flags,
      network::mojom::RequestDestination request_destination,
      bool has_user_gesture,
      bool originated_from_service_worker,
      CreateCheckerAndCheckCallback callback) override {}

  void Clone(mojo::PendingReceiver<mojom::SafeBrowsing> receiver) override {
    NOTREACHED();
  }
};

class SBRendererUrlLoaderThrottleTest : public ::testing::Test {
 protected:
  SBRendererUrlLoaderThrottleTest() : mojo_receiver_(&safe_browsing_) {
    mojo_receiver_.Bind(safe_browsing_remote_.BindNewPipeAndPassReceiver());
    throttle_ = std::make_unique<RendererURLLoaderThrottle>(
        safe_browsing_remote_.get(), MSG_ROUTING_NONE);
  }

  base::test::TaskEnvironment message_loop_;
  FakeSafeBrowsing safe_browsing_;
  mojo::Receiver<mojom::SafeBrowsing> mojo_receiver_;
  mojo::Remote<mojom::SafeBrowsing> safe_browsing_remote_;
  std::unique_ptr<RendererURLLoaderThrottle> throttle_;
};

TEST_F(SBRendererUrlLoaderThrottleTest, DefersHttpsUrl) {
  GURL url("https://example.com/");
  bool defer = false;
  network::ResourceRequest request;
  request.url = url;
  throttle_->WillStartRequest(&request, &defer);

  throttle_->WillProcessResponse(url, /*response_head=*/nullptr, &defer);
  EXPECT_TRUE(defer);
}

TEST_F(SBRendererUrlLoaderThrottleTest, DoesNotDeferChromeUrl) {
  GURL url("chrome://settings/");
  bool defer = false;
  network::ResourceRequest request;
  request.url = url;
  throttle_->WillStartRequest(&request, &defer);

  throttle_->WillProcessResponse(url, /*response_head=*/nullptr, &defer);
  EXPECT_FALSE(defer);
}

}  // namespace safe_browsing
