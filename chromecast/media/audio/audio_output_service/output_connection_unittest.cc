// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/audio_output_service/output_connection.h"

#include <memory>
#include <utility>

#include "base/files/scoped_file.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromecast/common/mojom/audio_socket.mojom.h"
#include "chromecast/media/audio/audio_output_service/audio_output_service.pb.h"
#include "chromecast/media/audio/audio_output_service/output_socket.h"
#include "chromecast/net/socket_util.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "net/base/io_buffer.h"
#include "net/socket/stream_socket.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {
namespace audio_output_service {

namespace {

using base::test::RunOnceCallback;
using testing::_;
using testing::Invoke;

MATCHER_P(EqualsProto, other, "") {
  return arg.SerializeAsString() == other.SerializeAsString();
}

enum MessageTypes : int {
  kTest = 1,
};

class MockAudioSocketBroker : public mojom::AudioSocketBroker {
 public:
  explicit MockAudioSocketBroker(
      mojo::PendingReceiver<mojom::AudioSocketBroker> pending_receiver)
      : receiver_(this, std::move(pending_receiver)) {}
  MockAudioSocketBroker(const MockAudioSocketBroker&) = delete;
  MockAudioSocketBroker& operator=(const MockAudioSocketBroker&) = delete;
  ~MockAudioSocketBroker() override = default;

  MOCK_METHOD(void,
              GetSocketDescriptor,
              (GetSocketDescriptorCallback),
              (override));

 private:
  mojo::Receiver<mojom::AudioSocketBroker> receiver_;
};

class MockOutputConnection : public OutputConnection {
 public:
  MockOutputConnection(
      mojo::PendingRemote<mojom::AudioSocketBroker> pending_socket_broker)
      : OutputConnection(std::move(pending_socket_broker)) {}
  MockOutputConnection(const MockOutputConnection&) = delete;
  MockOutputConnection& operator=(const MockOutputConnection&) = delete;
  ~MockOutputConnection() override = default;

  MOCK_METHOD(void, OnConnected, (std::unique_ptr<OutputSocket>), (override));
  MOCK_METHOD(void, OnConnectionFailed, (), (override));
};

class MockAudioSocketDelegate : public OutputSocket::Delegate {
 public:
  MockAudioSocketDelegate() = default;
  MockAudioSocketDelegate(const MockAudioSocketDelegate&) = delete;
  MockAudioSocketDelegate& operator=(const MockAudioSocketDelegate&) = delete;
  ~MockAudioSocketDelegate() override = default;

  MOCK_METHOD(bool, HandleMetadata, (const Generic&), (override));
  MOCK_METHOD(bool, HandleAudioData, (char*, size_t, int64_t), (override));
  MOCK_METHOD(bool,
              HandleAudioBuffer,
              (scoped_refptr<net::IOBuffer>, char*, size_t, int64_t),
              (override));
  MOCK_METHOD(void, OnConnectionError, (), (override));
};

}  // namespace

class OutputConnectionTest : public testing::Test {
 public:
  OutputConnectionTest() {
    mojo::PendingRemote<mojom::AudioSocketBroker> pending_socket_broker;
    audio_socket_broker_ = std::make_unique<MockAudioSocketBroker>(
        pending_socket_broker.InitWithNewPipeAndPassReceiver());
    output_connection_ = std::make_unique<MockOutputConnection>(
        std::move(pending_socket_broker));
  }
  ~OutputConnectionTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<MockAudioSocketBroker> audio_socket_broker_;
  std::unique_ptr<MockOutputConnection> output_connection_;
};

TEST_F(OutputConnectionTest, ConnectSucceed) {
  base::ScopedFD fd1, fd2;
  CreateUnnamedSocketPair(&fd1, &fd2);
  auto receiving_socket =
      std::make_unique<OutputSocket>(AdoptUnnamedSocketHandle(std::move(fd2)));
  auto receiving_socket_delegate = std::make_unique<MockAudioSocketDelegate>();
  receiving_socket->SetDelegate(receiving_socket_delegate.get());

  Generic message;
  message.mutable_set_playback_rate()->set_playback_rate(1.0);

  // Return a connected socket handle.
  EXPECT_CALL(*audio_socket_broker_, GetSocketDescriptor(_))
      .Times(1)
      .WillOnce(RunOnceCallback<0>(mojo::PlatformHandle(std::move(fd1))));

  // Verify the connection is successfully established, and the returned
  // OutputSocket is usable.
  EXPECT_CALL(*output_connection_, OnConnected(_))
      .Times(1)
      .WillOnce(Invoke([&message](std::unique_ptr<OutputSocket> socket) {
        EXPECT_TRUE(!!socket);
        EXPECT_TRUE(socket->SendProto(kTest, message));
      }));

  // Verify the delegate method can be triggered on the receiving end.
  EXPECT_CALL(*receiving_socket_delegate, HandleMetadata(EqualsProto(message)))
      .Times(1);

  output_connection_->Connect();
  task_environment_.RunUntilIdle();
}

TEST_F(OutputConnectionTest, ConnectFail) {
  EXPECT_CALL(*audio_socket_broker_, GetSocketDescriptor(_))
      .WillRepeatedly(
          [](mojom::AudioSocketBroker::GetSocketDescriptorCallback callback) {
            std::move(callback).Run(mojo::PlatformHandle());
          });
  EXPECT_CALL(*output_connection_, OnConnected(_)).Times(0);
  EXPECT_CALL(*output_connection_, OnConnectionFailed()).Times(1);

  output_connection_->Connect();
  task_environment_.FastForwardBy(base::Seconds(30));
  task_environment_.RunUntilIdle();
}

}  // namespace audio_output_service
}  // namespace media
}  // namespace chromecast
