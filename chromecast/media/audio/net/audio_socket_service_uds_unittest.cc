// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/net/audio_socket_service.h"

#include <memory>
#include <string>
#include <utility>

#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/posix/unix_domain_socket.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "chromecast/net/socket_util.h"
#include "net/base/net_errors.h"
#include "net/socket/stream_socket.h"
#include "net/socket/unix_domain_client_socket_posix.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {

using ::testing::_;
using ::testing::Invoke;

namespace {

constexpr char kTestSocket[] = "test.socket";
constexpr char kSocketMsg[] = "socket-handle";

class MockAudioSocketServiceDelegate : public AudioSocketService::Delegate {
 public:
  MockAudioSocketServiceDelegate() = default;
  MockAudioSocketServiceDelegate(const MockAudioSocketServiceDelegate&) =
      delete;
  MockAudioSocketServiceDelegate& operator=(
      const MockAudioSocketServiceDelegate&) = delete;
  ~MockAudioSocketServiceDelegate() override = default;

  MOCK_METHOD(void,
              HandleAcceptedSocket,
              (std::unique_ptr<net::StreamSocket>),
              (override));
};

}  // namespace

class AudioSocketServiceTest : public testing::TestWithParam<bool> {
 public:
  AudioSocketServiceTest() : use_socket_descriptor_(GetParam()) {
    EXPECT_TRUE(test_dir_.CreateUniqueTempDir());
    socket_path_ = test_dir_.GetPath().Append(kTestSocket).value();
  }

  ~AudioSocketServiceTest() override {
    io_thread_->task_runner()->DeleteSoon(FROM_HERE,
                                          std::move(audio_socket_service_));
  }

 protected:
  void InitializeAudioSocketService() {
    io_thread_ = std::make_unique<base::Thread>("test_io_thread");
    io_thread_->StartWithOptions(
        base::Thread::Options(base::MessagePumpType::IO, 0));
    io_thread_->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &AudioSocketServiceTest::InitializeAudioSocketServiceOnIoThread,
            base::Unretained(this)));
  }

  void InitializeAudioSocketServiceOnIoThread() {
    delegate_ = std::make_unique<MockAudioSocketServiceDelegate>();
    audio_socket_service_ = std::make_unique<AudioSocketService>(
        socket_path_, /*port=*/0, /*max_accept_loop=*/1, delegate_.get(),
        use_socket_descriptor_);
    audio_socket_service_->Accept();
  }

  void ConnectToAudioSocketService() {
    connecting_socket_ =
        std::make_unique<net::UnixDomainClientSocket>(socket_path_, true);
    int rv = connecting_socket_->Connect(base::BindOnce(
        &AudioSocketServiceTest::OnConnected, base::Unretained(this)));
    if (rv != net::ERR_IO_PENDING) {
      OnConnected(rv);
    } else {
      run_loop_.Run();
    }

    if (use_socket_descriptor_) {
      base::ScopedFD fd1, fd2;
      base::CreateSocketPair(&fd1, &fd2);
      connected_socket_ = AdoptUnnamedSocketHandle(std::move(fd1));
      base::UnixDomainSocket::SendMsg(
          connecting_socket_->ReleaseConnectedSocket(), kSocketMsg,
          sizeof(kSocketMsg), {fd2.get()});
    }
    run_loop_.Run();
  }

  void OnConnected(int rv) {
    EXPECT_EQ(rv, net::OK);
    if (run_loop_.running()) {
      run_loop_.Quit();
    }
  }

  const bool use_socket_descriptor_;
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  base::ScopedTempDir test_dir_;
  std::string socket_path_;
  base::RunLoop run_loop_;
  std::unique_ptr<base::Thread> io_thread_;
  std::unique_ptr<MockAudioSocketServiceDelegate> delegate_;
  std::unique_ptr<AudioSocketService> audio_socket_service_;
  std::unique_ptr<net::UnixDomainClientSocket> connecting_socket_;
  std::unique_ptr<net::StreamSocket> connected_socket_;
};

TEST_P(AudioSocketServiceTest, UseSocketDescriptor) {
  InitializeAudioSocketService();
  io_thread_->FlushForTesting();

  EXPECT_CALL(*delegate_, HandleAcceptedSocket(_))
      .WillOnce(Invoke([this](std::unique_ptr<net::StreamSocket> socket) {
        EXPECT_TRUE(socket);
        EXPECT_TRUE(socket->IsConnected());
        run_loop_.Quit();
      }));

  ConnectToAudioSocketService();
}

INSTANTIATE_TEST_SUITE_P(ReturnedSocketIsConnected,
                         AudioSocketServiceTest,
                         ::testing::Bool() /* use_socket_descriptor */
);

}  // namespace media
}  // namespace chromecast
