// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/audio_socket_broker.h"

#include <fcntl.h>
#include <sys/socket.h>

#include <cstring>
#include <memory>
#include <string>
#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/posix/eintr_wrapper.h"
#include "base/posix/unix_domain_socket.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "chromecast/net/socket_util.h"
#include "content/public/test/test_content_client_initializer.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"
#include "net/socket/stream_socket.h"
#include "net/socket/unix_domain_server_socket_posix.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {

namespace {

constexpr char kTestSocket[] = "test.socket";
constexpr char kSocketMsg[] = "socket-handle";
constexpr int kListenBacklog = 1;

}  // namespace

class AudioSocketBrokerTest : public content::RenderViewHostTestHarness {
 public:
  AudioSocketBrokerTest() = default;
  ~AudioSocketBrokerTest() override {
    if (io_thread_) {
      io_thread_->task_runner()->DeleteSoon(FROM_HERE,
                                            std::move(accepted_socket_));
      io_thread_->task_runner()->DeleteSoon(FROM_HERE,
                                            std::move(listen_socket_));
    }
  }

  void SetUp() override {
    ASSERT_TRUE(test_dir_.CreateUniqueTempDir());
    socket_path_ = test_dir_.GetPath().Append(kTestSocket).value();
    initializer_ = std::make_unique<content::TestContentClientInitializer>();
    content::RenderViewHostTestHarness::SetUp();
    audio_socket_broker_ = &AudioSocketBroker::CreateForTesting(
        *main_rfh(), audio_socket_broker_remote_.BindNewPipeAndPassReceiver(),
        socket_path_);
  }

  void SetupServerSocket() {
    base::WaitableEvent server_setup_finished;
    io_thread_ = std::make_unique<base::Thread>("test_io_thread");
    io_thread_->StartWithOptions(
        base::Thread::Options(base::MessagePumpType::IO, 0));
    io_thread_->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&AudioSocketBrokerTest::SetupServerSocketOnIoThread,
                       base::Unretained(this), &server_setup_finished));
    server_setup_finished.Wait();
  }

  void SetupServerSocketOnIoThread(base::WaitableEvent* server_setup_finished) {
    auto unix_socket = std::make_unique<net::UnixDomainServerSocket>(
        base::BindRepeating(
            [](const net::UnixDomainServerSocket::Credentials&) {
              // Always accept the connection.
              return true;
            }),
        /*use_abstract_namespace=*/true);
    int result = unix_socket->BindAndListen(socket_path_, kListenBacklog);
    EXPECT_EQ(result, net::OK);
    listen_socket_ = std::move(unix_socket);
    listen_socket_->AcceptSocketDescriptor(
        &accepted_descriptor_,
        base::BindRepeating(&AudioSocketBrokerTest::OnAccept,
                            base::Unretained(this)));
    server_setup_finished->Signal();
  }

  void OnAccept(int result) {
    EXPECT_EQ(result, net::OK);
    char buffer[16];
    std::vector<base::ScopedFD> fds;
    const int flags = fcntl(accepted_descriptor_, F_GETFL);
    ASSERT_NE(
        HANDLE_EINTR(fcntl(accepted_descriptor_, F_SETFL, flags & ~O_NONBLOCK)),
        -1);
    EXPECT_EQ(static_cast<size_t>(base::UnixDomainSocket::RecvMsg(
                  accepted_descriptor_, buffer, sizeof(buffer), &fds)),
              sizeof(kSocketMsg));
    EXPECT_EQ(memcmp(buffer, kSocketMsg, sizeof(kSocketMsg)), 0);
    EXPECT_THAT(fds, ::testing::SizeIs(1U));
    accepted_socket_ = AdoptUnnamedSocketHandle(std::move(fds[0]));
  }

  void OnSocketDescriptor(bool expect_success, mojo::PlatformHandle handle) {
    EXPECT_EQ(handle.is_valid_fd(), expect_success);
    descriptor_received_ = true;
    if (expect_success) {
      auto stream_socket = AdoptUnnamedSocketHandle(handle.TakeFD());
      EXPECT_TRUE(stream_socket->IsConnected());
    }
    run_loop_.Quit();
  }

  void RunThreadsUntilIdle() {
    run_loop_.Run();
    task_environment()->RunUntilIdle();
  }

 protected:
  mojo::Remote<mojom::AudioSocketBroker> audio_socket_broker_remote_;
  base::ScopedTempDir test_dir_;
  std::string socket_path_;
  std::unique_ptr<content::TestContentClientInitializer> initializer_;
  // `AudioSocketBroker` is a `DocumentService` which manages its own
  // lifecycle.
  AudioSocketBroker* audio_socket_broker_ = nullptr;
  bool descriptor_received_ = false;
  base::RunLoop run_loop_;

  std::unique_ptr<base::Thread> io_thread_;
  std::unique_ptr<net::UnixDomainServerSocket> listen_socket_;
  net::SocketDescriptor accepted_descriptor_;
  std::unique_ptr<net::StreamSocket> accepted_socket_;
};

TEST_F(AudioSocketBrokerTest, ValidSocketHandle) {
  SetupServerSocket();
  audio_socket_broker_remote_->GetSocketDescriptor(
      base::BindOnce(&AudioSocketBrokerTest::OnSocketDescriptor,
                     base::Unretained(this), true));
  RunThreadsUntilIdle();
  EXPECT_TRUE(descriptor_received_);
}

TEST_F(AudioSocketBrokerTest, InvalidSocketHandle) {
  audio_socket_broker_remote_->GetSocketDescriptor(
      base::BindOnce(&AudioSocketBrokerTest::OnSocketDescriptor,
                     base::Unretained(this), false));
  RunThreadsUntilIdle();
  EXPECT_TRUE(descriptor_received_);
}

}  // namespace media
}  // namespace chromecast
