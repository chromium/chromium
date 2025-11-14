// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/spotlight/remoting_client_io_proxy.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/check_op.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/test/bind.h"
#include "base/test/repeating_test_future.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_constants.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_frame_consumer.h"
#include "remoting/base/oauth_token_info.h"
#include "remoting/client/common/client_status_observer.h"
#include "remoting/proto/audio.pb.h"
#include "remoting/protocol/frame_consumer.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace ash::boca {
namespace {

constexpr std::string_view kConnectionCode = "123456789012";
constexpr std::string_view kAccessToken = "access_token";
constexpr std::string_view kAuthorizedHelperEmail = "test_user@gmail.com";

class TestObserver : public RemotingClientIOProxy::Observer {
 public:
  TestObserver() = default;
  ~TestObserver() override = default;

  // RemotingClientIOProxy::Observer:
  void OnCrdSessionEnded() override {
    crd_session_ended_future_.GetCallback().Run();
  }
  void OnStateUpdated(CrdConnectionState state) override {
    state_updated_future_.GetCallback().Run(state);
  }
  void OnFrameReceived(SkBitmap bitmap,
                       std::unique_ptr<webrtc::DesktopFrame> frame) override {
    frame_received_future_.GetCallback().Run(std::move(bitmap),
                                             std::move(frame));
  }
  void OnAudioPacketReceived(
      std::unique_ptr<remoting::AudioPacket> packet) override {
    audio_packet_received_future_.GetCallback().Run(std::move(packet));
  }

  base::test::TestFuture<void>& crd_session_ended_future() {
    return crd_session_ended_future_;
  }

  auto& frame_received_future() { return frame_received_future_; }

  auto& audio_packet_received_future() { return audio_packet_received_future_; }

  auto& state_updated_future() { return state_updated_future_; }

  base::WeakPtr<TestObserver> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::test::TestFuture<void> crd_session_ended_future_;
  base::test::RepeatingTestFuture<SkBitmap,
                                  std::unique_ptr<webrtc::DesktopFrame>>
      frame_received_future_;
  base::test::RepeatingTestFuture<std::unique_ptr<remoting::AudioPacket>>
      audio_packet_received_future_;
  base::test::RepeatingTestFuture<CrdConnectionState> state_updated_future_;
  base::WeakPtrFactory<TestObserver> weak_ptr_factory_{this};
};

class FakeRemotingClientWrapper
    : public RemotingClientIOProxyImpl::RemotingClientWrapper {
 public:
  FakeRemotingClientWrapper(
      base::OnceClosure quit_closure,
      std::unique_ptr<SpotlightFrameConsumer> frame_consumer,
      std::unique_ptr<SpotlightAudioStreamConsumer> audio_stream_consumer)
      : quit_closure_(std::move(quit_closure)),
        frame_consumer_(std::move(frame_consumer)),
        audio_stream_consumer_(std::move(audio_stream_consumer)) {}

  FakeRemotingClientWrapper(const FakeRemotingClientWrapper&) = delete;
  FakeRemotingClientWrapper& operator=(const FakeRemotingClientWrapper&) =
      delete;

  ~FakeRemotingClientWrapper() override = default;

  // RemotingClientIOProxyImpl::RemotingClientWrapper:
  void StartSession(std::string_view support_access_code,
                    remoting::OAuthTokenInfo oauth_token_info) override {
    support_access_code_ = std::string(support_access_code);
    oauth_token_info_ = oauth_token_info;
  }
  void StopSession() override {}
  void AddObserver(remoting::ClientStatusObserver* observer) override {
    client_status_observer_ = observer;
  }
  void RemoveObserver(remoting::ClientStatusObserver* observer) override {
    CHECK_EQ(observer, client_status_observer_);
    client_status_observer_ = nullptr;
  }

  const std::string& support_access_code() const {
    return support_access_code_;
  }

  const remoting::OAuthTokenInfo& oauth_token_info() const {
    return oauth_token_info_;
  }

  SpotlightFrameConsumer* frame_consumer() const {
    return frame_consumer_.get();
  }

  base::WeakPtr<remoting::protocol::AudioStub> audio_stub() const {
    return audio_stream_consumer_->GetWeakPtr();
  }

  remoting::ClientStatusObserver* client_status_observer() const {
    return client_status_observer_;
  }

  base::WeakPtr<FakeRemotingClientWrapper> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  base::OnceClosure TakeCrdSessionEndedCb() { return std::move(quit_closure_); }

 private:
  base::OnceClosure quit_closure_;
  std::unique_ptr<SpotlightFrameConsumer> frame_consumer_;
  std::unique_ptr<SpotlightAudioStreamConsumer> audio_stream_consumer_;

  std::string support_access_code_;
  remoting::OAuthTokenInfo oauth_token_info_;
  raw_ptr<remoting::ClientStatusObserver> client_status_observer_ = nullptr;
  base::WeakPtrFactory<FakeRemotingClientWrapper> weak_ptr_factory_{this};
};

class RemotingClientIOProxyImplTest : public testing::Test {
 protected:
  void SetUp() override {
    test_shared_url_loader_factory_ =
        base::MakeRefCounted<network::TestSharedURLLoaderFactory>();
    remoting_client_io_proxy_ = std::make_unique<RemotingClientIOProxyImpl>(
        test_shared_url_loader_factory_->Clone(),
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        /*create_remoting_client_wrapper_cb=*/
        base::BindLambdaForTesting(
            [this](base::OnceClosure quit_closure,
                   std::unique_ptr<SpotlightFrameConsumer> frame_consumer,
                   std::unique_ptr<SpotlightAudioStreamConsumer>
                       audio_stream_consumer,
                   scoped_refptr<network::SharedURLLoaderFactory>)
                -> std::unique_ptr<
                    RemotingClientIOProxyImpl::RemotingClientWrapper> {
              auto wrapper = std::make_unique<FakeRemotingClientWrapper>(
                  std::move(quit_closure), std::move(frame_consumer),
                  std::move(audio_stream_consumer));
              fake_remoting_client_wrapper_ = wrapper->GetWeakPtr();
              return wrapper;
            }));
  }

  base::test::ScopedFeatureList scoped_feature_list_{
      features::kBocaAudioForKiosk};
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  scoped_refptr<network::TestSharedURLLoaderFactory>
      test_shared_url_loader_factory_;
  std::unique_ptr<RemotingClientIOProxyImpl> remoting_client_io_proxy_;
  base::WeakPtr<FakeRemotingClientWrapper> fake_remoting_client_wrapper_ =
      nullptr;
};

TEST_F(RemotingClientIOProxyImplTest, StartCrdClient) {
  TestObserver observer;
  remoting_client_io_proxy_->StartCrdClient(
      std::string(kConnectionCode), std::string(kAccessToken),
      std::string(kAuthorizedHelperEmail), observer.GetWeakPtr());

  ASSERT_TRUE(fake_remoting_client_wrapper_);
  EXPECT_EQ(fake_remoting_client_wrapper_->support_access_code(),
            std::string(kConnectionCode));
  EXPECT_EQ(fake_remoting_client_wrapper_->oauth_token_info().access_token(),
            std::string(kAccessToken));
  EXPECT_EQ(fake_remoting_client_wrapper_->oauth_token_info().user_email(),
            std::string(kAuthorizedHelperEmail));

  fake_remoting_client_wrapper_->TakeCrdSessionEndedCb().Run();
  EXPECT_TRUE(observer.crd_session_ended_future().Wait());
}

TEST_F(RemotingClientIOProxyImplTest, StopCrdClient) {
  TestObserver observer;
  bool on_stopped_called = false;
  remoting_client_io_proxy_->StartCrdClient(
      std::string(kConnectionCode), std::string(kAccessToken),
      std::string(kAuthorizedHelperEmail), observer.GetWeakPtr());
  remoting_client_io_proxy_->StopCrdClient(base::BindLambdaForTesting(
      [&on_stopped_called]() { on_stopped_called = true; }));
  EXPECT_EQ(fake_remoting_client_wrapper_->client_status_observer(), nullptr);
  task_environment_.FastForwardBy(base::Seconds(3));

  EXPECT_TRUE(on_stopped_called);
}

TEST_F(RemotingClientIOProxyImplTest, OnFrameReceived) {
  TestObserver observer;
  remoting_client_io_proxy_->StartCrdClient(
      std::string(kConnectionCode), std::string(kAccessToken),
      std::string(kAuthorizedHelperEmail), observer.GetWeakPtr());
  fake_remoting_client_wrapper_->frame_consumer()->DrawFrame(
      std::make_unique<webrtc::BasicDesktopFrame>(webrtc::DesktopSize(1, 1)),
      base::DoNothing());

  EXPECT_TRUE(observer.frame_received_future().Wait());
}

TEST_F(RemotingClientIOProxyImplTest, OnAudioReceived) {
  TestObserver observer;
  remoting_client_io_proxy_->StartCrdClient(
      std::string(kConnectionCode), std::string(kAccessToken),
      std::string(kAuthorizedHelperEmail), observer.GetWeakPtr());
  fake_remoting_client_wrapper_->audio_stub()->ProcessAudioPacket(
      std::make_unique<remoting::AudioPacket>(), base::DoNothing());

  EXPECT_TRUE(observer.audio_packet_received_future().Wait());
}

TEST_F(RemotingClientIOProxyImplTest,
       StopCrdClientShouldNotRunNextCrdSessionEndedCb) {
  TestObserver first_observer;
  TestObserver second_observer;
  remoting_client_io_proxy_->StartCrdClient(
      std::string(kConnectionCode), std::string(kAccessToken),
      std::string(kAuthorizedHelperEmail), first_observer.GetWeakPtr());
  base::OnceClosure first_quit_closure =
      fake_remoting_client_wrapper_->TakeCrdSessionEndedCb();
  // Stop first CRD session then start a new one.
  remoting_client_io_proxy_->StopCrdClient(base::DoNothing());
  remoting_client_io_proxy_->StartCrdClient(
      std::string(kConnectionCode), std::string(kAccessToken),
      std::string(kAuthorizedHelperEmail), second_observer.GetWeakPtr());
  base::OnceClosure second_quit_closure =
      fake_remoting_client_wrapper_->TakeCrdSessionEndedCb();
  std::move(first_quit_closure).Run();
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(first_observer.crd_session_ended_future().Wait());
  EXPECT_FALSE(second_observer.crd_session_ended_future().IsReady());

  std::move(second_quit_closure).Run();
  EXPECT_TRUE(second_observer.crd_session_ended_future().Wait());
}

TEST_F(RemotingClientIOProxyImplTest, ConnectionStateConnected) {
  TestObserver observer;
  remoting_client_io_proxy_->StartCrdClient(
      std::string(kConnectionCode), std::string(kAccessToken),
      std::string(kAuthorizedHelperEmail), observer.GetWeakPtr());
  fake_remoting_client_wrapper_->client_status_observer()->OnConnected();

  EXPECT_EQ(observer.state_updated_future().Take(),
            CrdConnectionState::kConnected);
}

TEST_F(RemotingClientIOProxyImplTest, ConnectionStateFailed) {
  TestObserver observer;
  remoting_client_io_proxy_->StartCrdClient(
      std::string(kConnectionCode), std::string(kAccessToken),
      std::string(kAuthorizedHelperEmail), observer.GetWeakPtr());
  fake_remoting_client_wrapper_->client_status_observer()->OnConnectionFailed();

  EXPECT_EQ(observer.state_updated_future().Take(),
            CrdConnectionState::kFailed);
}

TEST_F(RemotingClientIOProxyImplTest, ConnectionStateDisconnected) {
  TestObserver observer;
  remoting_client_io_proxy_->StartCrdClient(
      std::string(kConnectionCode), std::string(kAccessToken),
      std::string(kAuthorizedHelperEmail), observer.GetWeakPtr());
  fake_remoting_client_wrapper_->client_status_observer()->OnDisconnected();

  EXPECT_EQ(observer.state_updated_future().Take(),
            CrdConnectionState::kDisconnected);
}

TEST_F(RemotingClientIOProxyImplTest, ConnectionStateDestroyed) {
  TestObserver observer;
  remoting_client_io_proxy_->StartCrdClient(
      std::string(kConnectionCode), std::string(kAccessToken),
      std::string(kAuthorizedHelperEmail), observer.GetWeakPtr());
  fake_remoting_client_wrapper_->client_status_observer()->OnClientDestroyed();

  EXPECT_EQ(observer.state_updated_future().Take(),
            CrdConnectionState::kDisconnected);
}

}  // namespace
}  // namespace ash::boca
