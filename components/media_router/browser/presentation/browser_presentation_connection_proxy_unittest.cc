// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/browser/presentation/browser_presentation_connection_proxy.h"

#include <memory>

#include "base/run_loop.h"
#include "components/media_router/browser/route_message_util.h"
#include "components/media_router/browser/test/mock_media_router.h"
#include "components/media_router/browser/test/test_helper.h"
#include "components/media_router/common/media_source.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"

using blink::mojom::PresentationConnectionMessage;
using blink::mojom::PresentationConnectionMessagePtr;
using media_router::mojom::RouteMessagePtr;
using ::testing::_;
using ::testing::Invoke;

namespace media_router {

namespace {

void ExpectMessage(const PresentationConnectionMessagePtr expected_message,
                   const PresentationConnectionMessagePtr message) {
  EXPECT_EQ(expected_message, message);
}

}  // namespace

constexpr char kMediaRouteId[] = "MockRouteId";

class BrowserPresentationConnectionProxyTest : public ::testing::Test {
 public:
  BrowserPresentationConnectionProxyTest() = default;

  void SetUp() override {
    mock_controller_connection_proxy_ =
        std::make_unique<MockPresentationConnectionProxy>();
    mojo::PendingRemote<blink::mojom::PresentationConnection>
        controller_connection_remote;
    receiver_ =
        std::make_unique<mojo::Receiver<blink::mojom::PresentationConnection>>(
            mock_controller_connection_proxy_.get(),
            controller_connection_remote.InitWithNewPipeAndPassReceiver());
    EXPECT_CALL(mock_router_, RegisterPresentationConnectionMessageObserver(_));
    EXPECT_CALL(
        *mock_controller_connection_proxy_,
        DidChangeState(blink::mojom::PresentationConnectionState::CONNECTED));

    mojo::Remote<blink::mojom::PresentationConnection>
        receiver_connection_remote;

    base::RunLoop run_loop;
    browser_connection_proxy_ =
        std::make_unique<BrowserPresentationConnectionProxy>(
            &mock_router_, "MockRouteId",
            receiver_connection_remote.BindNewPipeAndPassReceiver(),
            std::move(controller_connection_remote));
    run_loop.RunUntilIdle();
  }

  void TearDown() override {
    EXPECT_CALL(mock_router_,
                UnregisterPresentationConnectionMessageObserver(_));
    browser_connection_proxy_.reset();
    receiver_.reset();
    mock_controller_connection_proxy_.reset();
  }

  MockPresentationConnectionProxy* controller_connection_proxy() {
    return mock_controller_connection_proxy_.get();
  }

  BrowserPresentationConnectionProxy* browser_connection_proxy() {
    return browser_connection_proxy_.get();
  }

  MockMediaRouter* mock_router() { return &mock_router_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<MockPresentationConnectionProxy>
      mock_controller_connection_proxy_;
  std::unique_ptr<mojo::Receiver<blink::mojom::PresentationConnection>>
      receiver_;
  std::unique_ptr<BrowserPresentationConnectionProxy> browser_connection_proxy_;
  MockMediaRouter mock_router_;
};

TEST_F(BrowserPresentationConnectionProxyTest, TestOnMessageTextMessage) {
  std::string message = "test message";
  PresentationConnectionMessagePtr connection_message =
      PresentationConnectionMessage::NewMessage(message);

  EXPECT_CALL(*mock_router(), SendRouteMessage(kMediaRouteId, message));

  browser_connection_proxy()->OnMessage(std::move(connection_message));
}

TEST_F(BrowserPresentationConnectionProxyTest, TestOnMessageBinaryMessage) {
  std::vector<uint8_t> expected_data;
  expected_data.push_back(42);
  expected_data.push_back(36);

  PresentationConnectionMessagePtr connection_message =
      PresentationConnectionMessage::NewData(expected_data);

  EXPECT_CALL(*mock_router(), SendRouteBinaryMessage(_, _))
      .WillOnce([&expected_data](const MediaRoute::Id& route_id,
                                 std::unique_ptr<std::vector<uint8_t>> data) {
        EXPECT_EQ(*data, expected_data);
      });

  browser_connection_proxy()->OnMessage(std::move(connection_message));
}

TEST_F(BrowserPresentationConnectionProxyTest, OnMessagesReceived) {
  EXPECT_CALL(*controller_connection_proxy(), OnMessage(_))
      .WillOnce([&](auto message) {
        ExpectMessage(PresentationConnectionMessage::NewMessage("foo"),
                      std::move(message));
      })
      .WillOnce([&](auto message) {
        ExpectMessage(PresentationConnectionMessage::NewData(
                          std::vector<uint8_t>({1, 2, 3})),
                      std::move(message));
      });

  std::vector<RouteMessagePtr> route_messages;
  route_messages.emplace_back(message_util::RouteMessageFromString("foo"));
  route_messages.emplace_back(
      message_util::RouteMessageFromData(std::vector<uint8_t>({1, 2, 3})));
  browser_connection_proxy()->OnMessagesReceived(std::move(route_messages));
  base::RunLoop().RunUntilIdle();
}

}  // namespace media_router
