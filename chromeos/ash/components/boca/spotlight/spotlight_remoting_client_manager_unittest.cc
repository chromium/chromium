// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/spotlight/spotlight_remoting_client_manager.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/repeating_test_future.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/threading/sequence_bound.h"
#include "base/threading/thread.h"
#include "chromeos/ash/components/boca/spotlight/remoting_client_io_proxy.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_audio_stream_consumer.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_constants.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_frame_consumer.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_oauth_token_fetcher.h"
#include "remoting/proto/audio.pb.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace ash::boca {

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

constexpr std::string_view kValidConnectionCode = "123456789012";
constexpr std::string_view kInvalidConnectionCode = "123";
constexpr std::string_view kTestOAuthToken = "test-oauth-token";
constexpr std::string_view kTestRobotEmail = "robot@gserviceaccount.com";

class MockSpotlightOAuthTokenFetcher : public SpotlightOAuthTokenFetcher {
 public:
  MockSpotlightOAuthTokenFetcher() = default;
  ~MockSpotlightOAuthTokenFetcher() override = default;

  MOCK_METHOD(void, Start, (OAuthTokenCallback callback), (override));
  MOCK_METHOD(std::string, GetDeviceRobotEmail, (), (override));
};

class MockRemotingClientIOProxy : public RemotingClientIOProxy {
 public:
  MockRemotingClientIOProxy() = default;
  ~MockRemotingClientIOProxy() override = default;

  MOCK_METHOD(void,
              StartCrdClient,
              (std::string crd_connection_code,
               std::string oauth_access_token,
               std::string authorized_helper_email,
               base::WeakPtr<Observer> observer),
              (override));
  MOCK_METHOD(void, StopCrdClient, (base::OnceClosure), (override));
};

class SpotlightRemotingClientManagerImplTest : public testing::Test {
 protected:
  void SetUp() override {
    auto token_fetcher =
        std::make_unique<NiceMock<MockSpotlightOAuthTokenFetcher>>();
    token_fetcher_ = token_fetcher.get();
    ON_CALL(*token_fetcher_, GetDeviceRobotEmail)
        .WillByDefault(Return(std::string(kTestRobotEmail)));

    manager_ = std::make_unique<SpotlightRemotingClientManagerImpl>(
        std::move(token_fetcher), test_url_loader_factory_.GetSafeWeakWrapper(),
        base::BindRepeating(
            &SpotlightRemotingClientManagerImplTest::CreateTestRemotingIOProxy,
            base::Unretained(this)));
  }

  std::unique_ptr<RemotingClientIOProxy> CreateTestRemotingIOProxy(
      std::unique_ptr<network::PendingSharedURLLoaderFactory>
          pending_url_loader_factory,
      scoped_refptr<base::SequencedTaskRunner> observer_task_runner) {
    auto mock_proxy = std::make_unique<NiceMock<MockRemotingClientIOProxy>>();
    remoting_client_io_proxy_ = mock_proxy.get();
    return mock_proxy;
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<SpotlightRemotingClientManagerImpl> manager_;
  raw_ptr<NiceMock<MockSpotlightOAuthTokenFetcher>> token_fetcher_ = nullptr;
  raw_ptr<NiceMock<MockRemotingClientIOProxy>> remoting_client_io_proxy_ =
      nullptr;
};

TEST_F(SpotlightRemotingClientManagerImplTest,
       StartCrdClientWithInvalidCodeFails) {
  base::test::RepeatingTestFuture<CrdConnectionState> status_future;
  EXPECT_CALL(*token_fetcher_, Start).Times(0);
  manager_->StartCrdClient(std::string(kInvalidConnectionCode),
                           base::DoNothing(), base::DoNothing(),
                           base::DoNothing(), status_future.GetCallback());
  EXPECT_EQ(status_future.Take(), CrdConnectionState::kFailed);
}

TEST_F(SpotlightRemotingClientManagerImplTest, StartCrdClientFailsToGetToken) {
  base::test::RepeatingTestFuture<CrdConnectionState> status_future;
  EXPECT_CALL(*token_fetcher_, Start)
      .WillOnce(
          [](SpotlightOAuthTokenFetcher::OAuthTokenCallback oauth_callback) {
            std::move(oauth_callback).Run(std::nullopt);
          });
  manager_->StartCrdClient(std::string(kValidConnectionCode), base::DoNothing(),
                           base::DoNothing(), base::DoNothing(),
                           status_future.GetCallback());
  EXPECT_EQ(status_future.Take(), CrdConnectionState::kFailed);
}

TEST_F(SpotlightRemotingClientManagerImplTest, StartCrdClientSuccess) {
  base::RunLoop run_loop;
  base::WeakPtr<RemotingClientIOProxy::Observer> observer;
  base::test::TestFuture<void> stop_future;
  EXPECT_CALL(*token_fetcher_, Start)
      .WillOnce(
          [](SpotlightOAuthTokenFetcher::OAuthTokenCallback oauth_callback) {
            std::move(oauth_callback).Run(std::string(kTestOAuthToken));
          });
  EXPECT_CALL(*remoting_client_io_proxy_,
              StartCrdClient(std::string(kValidConnectionCode),
                             std::string(kTestOAuthToken),
                             std::string(kTestRobotEmail), _))
      .WillOnce([&run_loop, &observer](
                    std::string, std::string, std::string,
                    base::WeakPtr<RemotingClientIOProxy::Observer> obs) {
        observer = obs;
        run_loop.Quit();
      });
  manager_->StartCrdClient(std::string(kValidConnectionCode), base::DoNothing(),
                           base::DoNothing(), base::DoNothing(),
                           base::DoNothing());
  run_loop.Run();
  EXPECT_TRUE(observer);

  EXPECT_CALL(*remoting_client_io_proxy_, StopCrdClient)
      .WillOnce([](base::OnceClosure cb) { std::move(cb).Run(); });
  manager_->StopCrdClient(stop_future.GetCallback());
  // Observer should be invalidated on stop.
  EXPECT_FALSE(observer);
  EXPECT_TRUE(stop_future.Wait());
}

TEST_F(SpotlightRemotingClientManagerImplTest, GetDeviceRobotEmail) {
  EXPECT_CALL(*token_fetcher_, GetDeviceRobotEmail())
      .WillOnce(Return(std::string(kTestRobotEmail)));
  EXPECT_EQ(manager_->GetDeviceRobotEmail(), std::string(kTestRobotEmail));
}

TEST_F(SpotlightRemotingClientManagerImplTest, OnCrdSessionEnded) {
  base::RunLoop run_loop;
  base::test::TestFuture<void> session_ended_future;
  base::WeakPtr<RemotingClientIOProxy::Observer> observer;
  EXPECT_CALL(*token_fetcher_, Start)
      .WillOnce(
          [](SpotlightOAuthTokenFetcher::OAuthTokenCallback oauth_callback) {
            std::move(oauth_callback).Run(std::string(kTestOAuthToken));
          });
  EXPECT_CALL(*remoting_client_io_proxy_, StartCrdClient)
      .WillOnce([&run_loop, &observer](
                    std::string, std::string, std::string,
                    base::WeakPtr<RemotingClientIOProxy::Observer> obs) {
        observer = obs;
        run_loop.Quit();
      });
  manager_->StartCrdClient(
      std::string(kValidConnectionCode), session_ended_future.GetCallback(),
      base::DoNothing(), base::DoNothing(), base::DoNothing());
  run_loop.Run();
  observer->OnCrdSessionEnded();
  EXPECT_TRUE(session_ended_future.Wait());
}

TEST_F(SpotlightRemotingClientManagerImplTest, StatusUpdated) {
  base::RunLoop run_loop;
  base::test::RepeatingTestFuture<CrdConnectionState> status_updated_future;
  base::WeakPtr<RemotingClientIOProxy::Observer> observer;
  EXPECT_CALL(*token_fetcher_, Start)
      .WillOnce(
          [](SpotlightOAuthTokenFetcher::OAuthTokenCallback oauth_callback) {
            std::move(oauth_callback).Run(std::string(kTestOAuthToken));
          });
  EXPECT_CALL(*remoting_client_io_proxy_, StartCrdClient)
      .WillOnce([&run_loop, &observer](
                    std::string, std::string, std::string,
                    base::WeakPtr<RemotingClientIOProxy::Observer> obs) {
        observer = obs;
        run_loop.Quit();
      });
  manager_->StartCrdClient(std::string(kValidConnectionCode), base::DoNothing(),
                           base::DoNothing(), base::DoNothing(),
                           status_updated_future.GetCallback());
  run_loop.Run();
  observer->OnStateUpdated(CrdConnectionState::kConnected);
  EXPECT_EQ(status_updated_future.Take(), CrdConnectionState::kConnected);
}

TEST_F(SpotlightRemotingClientManagerImplTest, FrameReceived) {
  base::RunLoop run_loop;
  base::test::RepeatingTestFuture<SkBitmap,
                                  std::unique_ptr<webrtc::DesktopFrame>>
      frame_received_future;
  base::WeakPtr<RemotingClientIOProxy::Observer> observer;
  EXPECT_CALL(*token_fetcher_, Start)
      .WillOnce(
          [](SpotlightOAuthTokenFetcher::OAuthTokenCallback oauth_callback) {
            std::move(oauth_callback).Run(std::string(kTestOAuthToken));
          });
  EXPECT_CALL(*remoting_client_io_proxy_, StartCrdClient)
      .WillOnce([&run_loop, &observer](
                    std::string, std::string, std::string,
                    base::WeakPtr<RemotingClientIOProxy::Observer> obs) {
        observer = obs;
        run_loop.Quit();
      });
  manager_->StartCrdClient(std::string(kValidConnectionCode), base::DoNothing(),
                           frame_received_future.GetCallback(),
                           base::DoNothing(), base::DoNothing());
  run_loop.Run();
  observer->OnFrameReceived(SkBitmap(), nullptr);
  EXPECT_TRUE(frame_received_future.Wait());
}

TEST_F(SpotlightRemotingClientManagerImplTest, FrameReceivedTimeout) {
  base::RunLoop start_run_loop;
  base::RunLoop stop_run_loop;
  base::test::RepeatingTestFuture<SkBitmap,
                                  std::unique_ptr<webrtc::DesktopFrame>>
      frame_received_future;
  base::test::RepeatingTestFuture<CrdConnectionState> status_updated_future;
  base::WeakPtr<RemotingClientIOProxy::Observer> observer;
  EXPECT_CALL(*token_fetcher_, Start)
      .WillOnce(
          [](SpotlightOAuthTokenFetcher::OAuthTokenCallback oauth_callback) {
            std::move(oauth_callback).Run(std::string(kTestOAuthToken));
          });
  EXPECT_CALL(*remoting_client_io_proxy_, StartCrdClient)
      .WillOnce([&start_run_loop, &observer](
                    std::string, std::string, std::string,
                    base::WeakPtr<RemotingClientIOProxy::Observer> obs) {
        observer = obs;
        start_run_loop.Quit();
      });
  manager_->StartCrdClient(std::string(kValidConnectionCode), base::DoNothing(),
                           frame_received_future.GetCallback(),
                           base::DoNothing(),
                           status_updated_future.GetCallback());
  start_run_loop.Run();
  observer->OnFrameReceived(SkBitmap(), nullptr);
  EXPECT_TRUE(frame_received_future.Wait());

  EXPECT_CALL(*remoting_client_io_proxy_, StopCrdClient)
      .WillOnce([&stop_run_loop]() { stop_run_loop.Quit(); });
  task_environment_.FastForwardBy(base::Seconds(5));
  stop_run_loop.Run();
  EXPECT_EQ(status_updated_future.Take(), CrdConnectionState::kTimeout);
}

TEST_F(SpotlightRemotingClientManagerImplTest, CallStopCrdClientOnTimeout) {
  base::RunLoop run_loop;
  base::test::TestFuture<void> stop_future;
  base::test::RepeatingTestFuture<SkBitmap,
                                  std::unique_ptr<webrtc::DesktopFrame>>
      frame_received_future;
  std::optional<CrdConnectionState> updated_state;
  base::WeakPtr<RemotingClientIOProxy::Observer> observer;
  EXPECT_CALL(*token_fetcher_, Start)
      .WillOnce(
          [](SpotlightOAuthTokenFetcher::OAuthTokenCallback oauth_callback) {
            std::move(oauth_callback).Run(std::string(kTestOAuthToken));
          });
  EXPECT_CALL(*remoting_client_io_proxy_, StartCrdClient)
      .WillOnce([&observer, &run_loop](
                    std::string, std::string, std::string,
                    base::WeakPtr<RemotingClientIOProxy::Observer> obs) {
        observer = obs;
        run_loop.Quit();
      });
  manager_->StartCrdClient(
      std::string(kValidConnectionCode), base::DoNothing(),
      frame_received_future.GetCallback(), base::DoNothing(),
      base::BindLambdaForTesting(
          [this, &stop_future, &updated_state](CrdConnectionState state) {
            manager_->StopCrdClient(stop_future.GetCallback());
            updated_state = state;
          }));
  run_loop.Run();
  observer->OnFrameReceived(SkBitmap(), nullptr);
  EXPECT_TRUE(frame_received_future.Wait());
  EXPECT_CALL(*remoting_client_io_proxy_, StopCrdClient)
      .WillOnce([](base::OnceClosure cb) { std::move(cb).Run(); });
  task_environment_.FastForwardBy(base::Seconds(5));
  EXPECT_TRUE(stop_future.Wait());
  ASSERT_TRUE(updated_state.has_value());
  EXPECT_EQ(updated_state.value(), CrdConnectionState::kTimeout);
}

TEST_F(SpotlightRemotingClientManagerImplTest,
       FrameReceivedTimeoutResetsOnEachFrame) {
  base::RunLoop run_loop;
  base::test::RepeatingTestFuture<SkBitmap,
                                  std::unique_ptr<webrtc::DesktopFrame>>
      frame_received_future;
  base::WeakPtr<RemotingClientIOProxy::Observer> observer;
  EXPECT_CALL(*token_fetcher_, Start)
      .WillOnce(
          [](SpotlightOAuthTokenFetcher::OAuthTokenCallback oauth_callback) {
            std::move(oauth_callback).Run(std::string(kTestOAuthToken));
          });
  EXPECT_CALL(*remoting_client_io_proxy_, StartCrdClient)
      .WillOnce([&run_loop, &observer](
                    std::string, std::string, std::string,
                    base::WeakPtr<RemotingClientIOProxy::Observer> obs) {
        observer = obs;
        run_loop.Quit();
      });
  manager_->StartCrdClient(std::string(kValidConnectionCode), base::DoNothing(),
                           frame_received_future.GetCallback(),
                           base::DoNothing(), base::DoNothing());
  run_loop.Run();
  task_environment_.FastForwardBy(base::Seconds(3));
  observer->OnFrameReceived(SkBitmap(), nullptr);
  EXPECT_TRUE(frame_received_future.Wait());

  EXPECT_CALL(*remoting_client_io_proxy_, StopCrdClient).Times(0);
  task_environment_.FastForwardBy(base::Seconds(4));
  task_environment_.RunUntilIdle();
}

TEST_F(SpotlightRemotingClientManagerImplTest, AudioPacketReceived) {
  base::RunLoop run_loop;
  base::test::RepeatingTestFuture<std::unique_ptr<remoting::AudioPacket>>
      audio_packet_received_future;
  base::WeakPtr<RemotingClientIOProxy::Observer> observer;
  EXPECT_CALL(*token_fetcher_, Start)
      .WillOnce(
          [](SpotlightOAuthTokenFetcher::OAuthTokenCallback oauth_callback) {
            std::move(oauth_callback).Run(std::string(kTestOAuthToken));
          });
  EXPECT_CALL(*remoting_client_io_proxy_, StartCrdClient)
      .WillOnce([&run_loop, &observer](
                    std::string, std::string, std::string,
                    base::WeakPtr<RemotingClientIOProxy::Observer> obs) {
        observer = obs;
        run_loop.Quit();
      });
  manager_->StartCrdClient(
      std::string(kValidConnectionCode), base::DoNothing(), base::DoNothing(),
      audio_packet_received_future.GetCallback(), base::DoNothing());
  run_loop.Run();
  observer->OnAudioPacketReceived(nullptr);
  EXPECT_TRUE(audio_packet_received_future.Wait());
}

}  // namespace ash::boca
