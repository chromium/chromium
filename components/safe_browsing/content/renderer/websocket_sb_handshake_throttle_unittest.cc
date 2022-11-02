// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/renderer/websocket_sb_handshake_throttle.h"

#include <utility>

#include "base/callback.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/safe_browsing/content/common/safe_browsing.mojom.h"
#include "components/safe_browsing/core/common/safe_browsing_url_checker.mojom.h"
#include "ipc/ipc_message.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"

namespace safe_browsing {

namespace {

constexpr char kTestUrl[] = "wss://test/";

class FakeSafeBrowsing : public mojom::SafeBrowsing {
 public:
  FakeSafeBrowsing()
      : render_frame_id_(),
        load_flags_(-1),
        request_destination_(),
        has_user_gesture_(false),
        originated_from_service_worker_(false) {}

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
      CreateCheckerAndCheckCallback callback) override {
    render_frame_id_ = render_frame_id;
    receiver_ = std::move(receiver);
    url_ = url;
    method_ = method;
    headers_ = headers;
    load_flags_ = load_flags;
    request_destination_ = request_destination;
    has_user_gesture_ = has_user_gesture;
    originated_from_service_worker_ = originated_from_service_worker;
    callback_ = std::move(callback);
    run_loop_.Quit();
  }

  void Clone(mojo::PendingReceiver<mojom::SafeBrowsing> receiver) override {
    NOTREACHED();
  }

  void RunUntilCalled() { run_loop_.Run(); }

  int32_t render_frame_id_;
  mojo::PendingReceiver<mojom::SafeBrowsingUrlChecker> receiver_;
  GURL url_;
  std::string method_;
  net::HttpRequestHeaders headers_;
  int32_t load_flags_;
  network::mojom::RequestDestination request_destination_;
  bool has_user_gesture_;
  bool originated_from_service_worker_;
  CreateCheckerAndCheckCallback callback_;
  base::RunLoop run_loop_;
};

class FakeCallback {
 public:
  enum Result { RESULT_NOT_CALLED, RESULT_SUCCESS, RESULT_ERROR };

  FakeCallback() : result_(RESULT_NOT_CALLED) {}

  void OnCompletion(const absl::optional<blink::WebString>& message) {
    if (message) {
      result_ = RESULT_ERROR;
      message_ = *message;
      run_loop_.Quit();
      return;
    }

    result_ = RESULT_SUCCESS;
    run_loop_.Quit();
  }

  void RunUntilCalled() { run_loop_.Run(); }

  void RunUntilIdle() { base::RunLoop().RunUntilIdle(); }

  Result result_;
  blink::WebString message_;
  base::RunLoop run_loop_;
};

class WebSocketSBHandshakeThrottleTest : public ::testing::Test {
 protected:
  WebSocketSBHandshakeThrottleTest() : mojo_receiver_(&safe_browsing_) {
    mojo_receiver_.Bind(safe_browsing_remote_.BindNewPipeAndPassReceiver());
    throttle_ = std::make_unique<WebSocketSBHandshakeThrottle>(
        safe_browsing_remote_.get(), MSG_ROUTING_NONE);
  }

  base::test::TaskEnvironment message_loop_;
  FakeSafeBrowsing safe_browsing_;
  mojo::Receiver<mojom::SafeBrowsing> mojo_receiver_;
  mojo::Remote<mojom::SafeBrowsing> safe_browsing_remote_;
  std::unique_ptr<WebSocketSBHandshakeThrottle> throttle_;
  FakeCallback fake_callback_;
};

TEST_F(WebSocketSBHandshakeThrottleTest, Construction) {}

TEST_F(WebSocketSBHandshakeThrottleTest, CheckArguments) {
  throttle_->ThrottleHandshake(
      GURL(kTestUrl), base::BindOnce(&FakeCallback::OnCompletion,
                                     base::Unretained(&fake_callback_)));
  safe_browsing_.RunUntilCalled();
  EXPECT_EQ(MSG_ROUTING_NONE, safe_browsing_.render_frame_id_);
  EXPECT_EQ(GURL(kTestUrl), safe_browsing_.url_);
  EXPECT_EQ("GET", safe_browsing_.method_);
  EXPECT_TRUE(safe_browsing_.headers_.GetHeaderVector().empty());
  EXPECT_EQ(0, safe_browsing_.load_flags_);
  EXPECT_EQ(network::mojom::RequestDestination::kEmpty,
            safe_browsing_.request_destination_);
  EXPECT_FALSE(safe_browsing_.has_user_gesture_);
  EXPECT_FALSE(safe_browsing_.originated_from_service_worker_);
  EXPECT_TRUE(safe_browsing_.callback_);
}

TEST_F(WebSocketSBHandshakeThrottleTest, Safe) {
  throttle_->ThrottleHandshake(
      GURL(kTestUrl), base::BindOnce(&FakeCallback::OnCompletion,
                                     base::Unretained(&fake_callback_)));
  safe_browsing_.RunUntilCalled();
  std::move(safe_browsing_.callback_)
      .Run(mojo::NullReceiver(), true, false, false, false);
  fake_callback_.RunUntilCalled();
  EXPECT_EQ(FakeCallback::RESULT_SUCCESS, fake_callback_.result_);
}

TEST_F(WebSocketSBHandshakeThrottleTest, Unsafe) {
  throttle_->ThrottleHandshake(
      GURL(kTestUrl), base::BindOnce(&FakeCallback::OnCompletion,
                                     base::Unretained(&fake_callback_)));
  safe_browsing_.RunUntilCalled();
  std::move(safe_browsing_.callback_)
      .Run(mojo::NullReceiver(), false, false, false, false);
  fake_callback_.RunUntilCalled();
  EXPECT_EQ(FakeCallback::RESULT_ERROR, fake_callback_.result_);
  EXPECT_EQ(
      blink::WebString(
          "WebSocket connection to wss://test/ failed safe browsing check"),
      fake_callback_.message_);
}

TEST_F(WebSocketSBHandshakeThrottleTest, SlowCheckNotifier) {
  throttle_->ThrottleHandshake(
      GURL(kTestUrl), base::BindOnce(&FakeCallback::OnCompletion,
                                     base::Unretained(&fake_callback_)));
  safe_browsing_.RunUntilCalled();

  mojo::Remote<mojom::UrlCheckNotifier> slow_check_notifier;
  std::move(safe_browsing_.callback_)
      .Run(slow_check_notifier.BindNewPipeAndPassReceiver(), false, false,
           false, false);
  fake_callback_.RunUntilIdle();
  EXPECT_EQ(FakeCallback::RESULT_NOT_CALLED, fake_callback_.result_);

  slow_check_notifier->OnCompleteCheck(true, false, false, false);
  fake_callback_.RunUntilCalled();
  EXPECT_EQ(FakeCallback::RESULT_SUCCESS, fake_callback_.result_);
}

TEST_F(WebSocketSBHandshakeThrottleTest, MojoServiceNotThere) {
  mojo_receiver_.reset();
  throttle_->ThrottleHandshake(
      GURL(kTestUrl), base::BindOnce(&FakeCallback::OnCompletion,
                                     base::Unretained(&fake_callback_)));
  fake_callback_.RunUntilCalled();
  EXPECT_EQ(FakeCallback::RESULT_SUCCESS, fake_callback_.result_);
}

}  // namespace

}  // namespace safe_browsing
