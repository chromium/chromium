// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/renderer/websocket_sb_extensions_handshake_throttle.h"

#include <optional>
#include <utility>

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/safe_browsing/content/common/safe_browsing.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"

namespace safe_browsing {

namespace {

constexpr char kTestUrl[] = "wss://test/";
constexpr char kTestExtensionId[] = "abcdefghijklmnopabcdefghijklmnop";
constexpr char kTestExtensionUrl[] =
    "chrome-extension://abcdefghijklmnopabcdefghijklmnop/";

class FakeExtensionWebRequestReporter
    : public mojom::ExtensionWebRequestReporter {
 public:
  FakeExtensionWebRequestReporter() {}

  void SendWebRequestData(
      const std::string& origin_extension_id,
      const GURL& telemetry_url,
      mojom::WebRequestProtocolType protocol_type,
      mojom::WebRequestContactInitiatorType contact_initiator_type) override {
    origin_extension_id_ = origin_extension_id;
    telemetry_url_ = telemetry_url;
    protocol_type_ = protocol_type;
    contact_initiator_type_ = contact_initiator_type;
    run_loop_.Quit();
  }

  void Clone(mojo::PendingReceiver<mojom::ExtensionWebRequestReporter> receiver)
      override {
    NOTREACHED();
  }

  void RunUntilCalled() { run_loop_.Run(); }

  std::string origin_extension_id_;
  GURL telemetry_url_;
  mojom::WebRequestProtocolType protocol_type_;
  mojom::WebRequestContactInitiatorType contact_initiator_type_;
  base::RunLoop run_loop_;
};

class FakeCallback {
 public:
  enum Result { RESULT_NOT_CALLED, RESULT_SUCCESS, RESULT_ERROR };

  FakeCallback() : result_(RESULT_NOT_CALLED) {}

  void OnCompletion(const std::optional<blink::WebString>& message) {
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

class WebSocketSBExtensionsHandshakeThrottleTest : public ::testing::Test {
 protected:
  WebSocketSBExtensionsHandshakeThrottleTest()
      : extension_web_request_reporter_receiver_(
            &extension_web_request_reporter_) {
    extension_web_request_reporter_receiver_.Bind(
        extension_web_request_reporter_remote_.BindNewPipeAndPassReceiver());
    throttle_ = std::make_unique<WebSocketSBExtensionsHandshakeThrottle>(
        extension_web_request_reporter_remote_.get());
  }

  base::test::TaskEnvironment message_loop_;
  FakeExtensionWebRequestReporter extension_web_request_reporter_;
  mojo::Receiver<mojom::ExtensionWebRequestReporter>
      extension_web_request_reporter_receiver_;
  mojo::Remote<mojom::ExtensionWebRequestReporter>
      extension_web_request_reporter_remote_;
  std::unique_ptr<WebSocketSBExtensionsHandshakeThrottle> throttle_;
  FakeCallback fake_callback_;
};

TEST_F(WebSocketSBExtensionsHandshakeThrottleTest, Construction) {}

TEST_F(WebSocketSBExtensionsHandshakeThrottleTest,
       SendExtensionWebRequestData) {
  base::HistogramTester histogram_tester;
  throttle_->ThrottleHandshake(
      GURL(kTestUrl),
      blink::WebSecurityOrigin::CreateFromString(kTestExtensionUrl),
      blink::WebSecurityOrigin(),
      base::BindOnce(&FakeCallback::OnCompletion,
                     base::Unretained(&fake_callback_)));
  EXPECT_EQ(FakeCallback::RESULT_SUCCESS, fake_callback_.result_);
  extension_web_request_reporter_.RunUntilCalled();

  EXPECT_EQ(extension_web_request_reporter_.origin_extension_id_,
            kTestExtensionId);
  EXPECT_EQ(extension_web_request_reporter_.telemetry_url_, GURL(kTestUrl));
  EXPECT_EQ(extension_web_request_reporter_.protocol_type_,
            mojom::WebRequestProtocolType::kWebSocket);
  EXPECT_EQ(extension_web_request_reporter_.contact_initiator_type_,
            mojom::WebRequestContactInitiatorType::kExtension);

  // A log of "false" represents the data being sent.
  histogram_tester.ExpectBucketCount(
      "SafeBrowsing.ExtensionTelemetry.WebSocketRequestDataSentOrReceived",
      false, 1);
}

TEST_F(WebSocketSBExtensionsHandshakeThrottleTest,
       SendExtensionWebRequestData_ContentScript) {
  base::HistogramTester histogram_tester;
  throttle_->ThrottleHandshake(
      GURL(kTestUrl), blink::WebSecurityOrigin(),
      blink::WebSecurityOrigin::CreateFromString(kTestExtensionUrl),
      base::BindOnce(&FakeCallback::OnCompletion,
                     base::Unretained(&fake_callback_)));
  EXPECT_EQ(FakeCallback::RESULT_SUCCESS, fake_callback_.result_);
  extension_web_request_reporter_.RunUntilCalled();

  EXPECT_EQ(extension_web_request_reporter_.origin_extension_id_,
            kTestExtensionId);
  EXPECT_EQ(extension_web_request_reporter_.telemetry_url_, GURL(kTestUrl));
  EXPECT_EQ(extension_web_request_reporter_.protocol_type_,
            mojom::WebRequestProtocolType::kWebSocket);
  EXPECT_EQ(extension_web_request_reporter_.contact_initiator_type_,
            mojom::WebRequestContactInitiatorType::kContentScript);

  // A log of "false" represents the data being sent.
  histogram_tester.ExpectBucketCount(
      "SafeBrowsing.ExtensionTelemetry.WebSocketRequestDataSentOrReceived",
      false, 1);
}

}  // namespace

}  // namespace safe_browsing
