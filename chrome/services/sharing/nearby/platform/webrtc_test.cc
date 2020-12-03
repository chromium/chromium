// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/webrtc.h"

#include "base/i18n/timezone.h"
#include "base/test/task_environment.h"
#include "chrome/services/sharing/nearby/test_support/mock_webrtc_dependencies.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace location {
namespace nearby {
namespace chrome {
namespace {

class MockPeerConnectionObserver : public webrtc::PeerConnectionObserver {
 public:
  MOCK_METHOD(void, OnRenegotiationNeeded, (), (override));
  MOCK_METHOD(void,
              OnIceCandidate,
              (const webrtc::IceCandidateInterface*),
              (override));
  MOCK_METHOD(void,
              OnDataChannel,
              (rtc::scoped_refptr<webrtc::DataChannelInterface>),
              (override));
  MOCK_METHOD(void,
              OnIceGatheringChange,
              (webrtc::PeerConnectionInterface::IceGatheringState new_state),
              (override));
  MOCK_METHOD(void,
              OnSignalingChange,
              (webrtc::PeerConnectionInterface::SignalingState new_state),
              (override));
};

class WebRtcMediumTest : public ::testing::Test {
 public:
  WebRtcMediumTest()
      : socket_manager_(mojo_impl_.socket_manager_.BindNewPipeAndPassRemote(),
                        task_environment_.GetMainThreadTaskRunner()),
        mdns_responder_(mojo_impl_.mdns_responder_.BindNewPipeAndPassRemote(),
                        task_environment_.GetMainThreadTaskRunner()),
        ice_config_fetcher_(
            mojo_impl_.ice_config_fetcher_.BindNewPipeAndPassRemote()),
        messenger_(mojo_impl_.messenger_.BindNewPipeAndPassRemote()),
        webrtc_medium_(socket_manager_,
                       mdns_responder_,
                       ice_config_fetcher_,
                       messenger_,
                       base::ThreadTaskRunnerHandle::Get()) {}

  ~WebRtcMediumTest() override {
    // Let libjingle threads finish.
    base::RunLoop().RunUntilIdle();
  }

  WebRtcMedium& GetMedium() { return webrtc_medium_; }

  testing::NiceMock<sharing::MockWebRtcDependencies>&
  GetMockWebRtcDependencies() {
    return mojo_impl_;
  }

  connections::LocationHint GetCountryCodeLocationHint(
      const std::string& country_code) {
    auto location_hint = connections::LocationHint();
    location_hint.set_location(country_code);
    location_hint.set_format(
        connections::LocationStandard_Format_ISO_3166_1_ALPHA_2);
    return location_hint;
  }

  connections::LocationHint GetCallingCodeLocationHint(
      const std::string& calling_code) {
    auto location_hint = connections::LocationHint();
    location_hint.set_location(calling_code);
    location_hint.set_format(connections::LocationStandard_Format_E164_CALLING);
    return location_hint;
  }

  connections::LocationHint GetUnknownLocationHint() {
    auto location_hint = connections::LocationHint();
    location_hint.set_location("");
    location_hint.set_format(connections::LocationStandard_Format_UNKNOWN);
    return location_hint;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  testing::NiceMock<sharing::MockWebRtcDependencies> mojo_impl_;

  mojo::SharedRemote<network::mojom::P2PSocketManager> socket_manager_;
  mojo::SharedRemote<network::mojom::MdnsResponder> mdns_responder_;
  mojo::SharedRemote<sharing::mojom::IceConfigFetcher> ice_config_fetcher_;
  mojo::SharedRemote<sharing::mojom::WebRtcSignalingMessenger> messenger_;

  WebRtcMedium webrtc_medium_;
};

TEST_F(WebRtcMediumTest, CreatePeerConnection) {
  MockPeerConnectionObserver observer;
  ON_CALL(GetMockWebRtcDependencies(), GetIceServers(testing::_))
      .WillByDefault(testing::Invoke(
          [](sharing::MockWebRtcDependencies::GetIceServersCallback callback) {
            std::move(callback).Run({});
          }));
  EXPECT_CALL(GetMockWebRtcDependencies(), GetIceServers(testing::_));

  base::RunLoop loop;
  GetMedium().CreatePeerConnection(
      &observer, [&](rtc::scoped_refptr<webrtc::PeerConnectionInterface> pc) {
        ASSERT_TRUE(pc);
        pc->Close();
        loop.Quit();
      });
  loop.Run();
}

TEST_F(WebRtcMediumTest, GetSignalingMessenger) {
  std::unique_ptr<api::WebRtcSignalingMessenger> messenger =
      GetMedium().GetSignalingMessenger("from",
                                        GetCountryCodeLocationHint("ZZ"));
  EXPECT_TRUE(messenger);
}

TEST_F(WebRtcMediumTest, GetMessengerAndSendMessage) {
  ByteArray message("message");
  const std::string from = "from";
  const std::string to = "to";

  base::RunLoop loop;
  EXPECT_CALL(GetMockWebRtcDependencies(),
              SendMessage(testing::Eq(from), testing::Eq(to), testing::_,
                          testing::Eq(std::string(message)), testing::_))
      .WillOnce(testing::WithArg<4>(testing::Invoke(
          [&](sharing::MockWebRtcDependencies::SendMessageCallback callback) {
            std::move(callback).Run(/*success=*/true);
            loop.Quit();
          })));

  std::unique_ptr<api::WebRtcSignalingMessenger> messenger =
      GetMedium().GetSignalingMessenger(from, GetCountryCodeLocationHint("ZZ"));
  EXPECT_TRUE(messenger);

  EXPECT_TRUE(messenger->SendMessage(to, message));
  loop.Run();
}

TEST_F(WebRtcMediumTest, GetMessengerAndSendMessageWithUnknownLocationHint) {
  ByteArray message("message");
  const std::string from = "from";
  const std::string to = "to";

  base::RunLoop loop;
  EXPECT_CALL(GetMockWebRtcDependencies(),
              SendMessage(testing::Eq(from), testing::Eq(to), testing::_,
                          testing::Eq(std::string(message)), testing::_))
      .WillOnce(testing::Invoke(
          [&](const std::string& self_id, const std::string& peer_id,
              sharing::mojom::LocationHintPtr location_hint,
              const std::string& message,
              sharing::MockWebRtcDependencies::SendMessageCallback callback) {
            // Validate we get the default country code if we pass an UNKNOWN
            // location hint.
            EXPECT_EQ(base::CountryCodeForCurrentTimezone(),
                      location_hint->location);
            EXPECT_EQ(
                sharing::mojom::LocationStandardFormat::ISO_3166_1_ALPHA_2,
                location_hint->format);
            std::move(callback).Run(/*success=*/true);
            loop.Quit();
          }));

  std::unique_ptr<api::WebRtcSignalingMessenger> messenger =
      GetMedium().GetSignalingMessenger(from, GetUnknownLocationHint());
  EXPECT_TRUE(messenger);

  EXPECT_TRUE(messenger->SendMessage(to, message));
  loop.Run();
}

TEST_F(WebRtcMediumTest, GetMessengerAndStartReceivingMessages) {
  ByteArray message("message");
  const std::string from = "from";

  EXPECT_CALL(GetMockWebRtcDependencies(),
              StartReceivingMessages(testing::Eq(from), testing::_, testing::_,
                                     testing::_))
      .WillOnce(testing::Invoke(
          [&message](
              const std::string& self_id,
              sharing::mojom::LocationHintPtr location_hint,
              mojo::PendingRemote<sharing::mojom::IncomingMessagesListener>
                  listener,
              sharing::MockWebRtcDependencies::StartReceivingMessagesCallback
                  callback) {
            EXPECT_EQ("ZZ", location_hint->location);
            EXPECT_EQ(
                sharing::mojom::LocationStandardFormat::ISO_3166_1_ALPHA_2,
                location_hint->format);
            std::move(callback).Run(/*success=*/true);
            mojo::Remote<sharing::mojom::IncomingMessagesListener> remote(
                std::move(listener));
            remote->OnMessage(std::string(message));
          }));

  std::unique_ptr<api::WebRtcSignalingMessenger> messenger =
      GetMedium().GetSignalingMessenger(from, GetCountryCodeLocationHint("ZZ"));
  EXPECT_TRUE(messenger);

  base::RunLoop loop;
  EXPECT_TRUE(messenger->StartReceivingMessages([&](const ByteArray& msg) {
    EXPECT_EQ(message, msg);
    loop.Quit();
  }));
  loop.Run();
}

// TODO(crbug.com/1146543): Test is flaky.
TEST_F(WebRtcMediumTest, DISABLED_GetMessenger_StartAndStopReceivingMessages) {
  ByteArray message("message");
  const std::string from = "from";

  mojo::Remote<sharing::mojom::IncomingMessagesListener> remote;
  EXPECT_CALL(GetMockWebRtcDependencies(),
              StartReceivingMessages(testing::Eq(from), testing::_, testing::_,
                                     testing::_))
      .WillOnce(testing::Invoke(
          [&](const std::string& self_id,
              sharing::mojom::LocationHintPtr location_hint,
              mojo::PendingRemote<sharing::mojom::IncomingMessagesListener>
                  listener,
              sharing::MockWebRtcDependencies::StartReceivingMessagesCallback
                  callback) {
            // Expect the unknown location hint to get defaulted by the time we
            // get here.
            EXPECT_EQ(base::CountryCodeForCurrentTimezone(),
                      location_hint->location);
            EXPECT_EQ(
                sharing::mojom::LocationStandardFormat::ISO_3166_1_ALPHA_2,
                location_hint->format);

            std::move(callback).Run(/*success=*/true);

            remote.Bind(std::move(listener));
            remote->OnMessage(std::string(message));
          }));
  EXPECT_CALL(GetMockWebRtcDependencies(), StopReceivingMessages())
      .WillRepeatedly(testing::Invoke([&]() {
        if (remote.is_bound()) {
          remote.reset();
        }
      }));

  std::unique_ptr<api::WebRtcSignalingMessenger> messenger =
      GetMedium().GetSignalingMessenger(from, GetUnknownLocationHint());
  EXPECT_TRUE(messenger);

  base::RunLoop loop;
  EXPECT_TRUE(messenger->StartReceivingMessages([&](const ByteArray& msg) {
    EXPECT_EQ(message, msg);
    loop.Quit();
  }));
  loop.Run();

  EXPECT_TRUE(remote.is_connected());

  messenger->StopReceivingMessages();
  // Run mojo disconnect handlers.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(remote.is_bound());
}

TEST_F(WebRtcMediumTest, GetMessengerAndStartReceivingMessagesTwice) {
  ByteArray message("message");
  const std::string from = "from";

  EXPECT_CALL(GetMockWebRtcDependencies(),
              StartReceivingMessages(testing::Eq(from), testing::_, testing::_,
                                     testing::_))
      .WillOnce(testing::Invoke(
          [&message](
              const std::string& self_id,
              sharing::mojom::LocationHintPtr location_hint,
              mojo::PendingRemote<sharing::mojom::IncomingMessagesListener>
                  listener,
              sharing::MockWebRtcDependencies::StartReceivingMessagesCallback
                  callback) {
            EXPECT_EQ("+1", location_hint->location);
            EXPECT_EQ(sharing::mojom::LocationStandardFormat::E164_CALLING,
                      location_hint->format);

            std::move(callback).Run(/*success=*/true);

            mojo::Remote<sharing::mojom::IncomingMessagesListener> remote(
                std::move(listener));
            remote->OnMessage(std::string(message));
          }));

  std::unique_ptr<api::WebRtcSignalingMessenger> messenger =
      GetMedium().GetSignalingMessenger(from, GetCallingCodeLocationHint("+1"));
  EXPECT_TRUE(messenger);

  base::RunLoop loop;
  EXPECT_TRUE(messenger->StartReceivingMessages([&](const ByteArray& msg) {
    EXPECT_EQ(message, msg);
    loop.Quit();
  }));
  loop.Run();

  message = ByteArray("message_2");
  EXPECT_CALL(GetMockWebRtcDependencies(),
              StartReceivingMessages(testing::Eq(from), testing::_, testing::_,
                                     testing::_))
      .WillOnce(testing::Invoke(
          [&message](
              const std::string& self_id,
              sharing::mojom::LocationHintPtr location_hint,
              mojo::PendingRemote<sharing::mojom::IncomingMessagesListener>
                  listener,
              sharing::MockWebRtcDependencies::StartReceivingMessagesCallback
                  callback) {
            EXPECT_EQ("+1", location_hint->location);
            EXPECT_EQ(sharing::mojom::LocationStandardFormat::E164_CALLING,
                      location_hint->format);

            std::move(callback).Run(/*success=*/true);

            mojo::Remote<sharing::mojom::IncomingMessagesListener> remote(
                std::move(listener));
            remote->OnMessage(std::string(message));
          }));

  base::RunLoop loop_2;
  EXPECT_TRUE(messenger->StartReceivingMessages([&](const ByteArray& msg) {
    EXPECT_EQ(message, msg);
    loop_2.Quit();
  }));
  loop_2.Run();
}

}  // namespace
}  // namespace chrome
}  // namespace nearby
}  // namespace location
