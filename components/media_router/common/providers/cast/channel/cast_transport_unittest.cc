// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/common/providers/cast/channel/cast_transport.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/containers/queue.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "components/media_router/common/providers/cast/channel/cast_framer.h"
#include "components/media_router/common/providers/cast/channel/cast_socket.h"
#include "components/media_router/common/providers/cast/channel/cast_test_util.h"
#include "components/media_router/common/providers/cast/channel/logger.h"
#include "net/base/completion_once_callback.h"
#include "net/base/completion_repeating_callback.h"
#include "net/base/net_errors.h"
#include "net/log/test_net_log.h"
#include "net/socket/socket.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/network_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/openscreen/src/cast/common/channel/proto/cast_channel.pb.h"

using testing::_;
using testing::DoAll;
using testing::InSequence;
using testing::Invoke;
using testing::NotNull;
using testing::Return;
using testing::WithArg;

namespace cast_channel {
namespace {

const int kChannelId = 0;

// Mockable placeholder for write completion events.
class CompleteHandler {
 public:
  CompleteHandler() = default;

  CompleteHandler(const CompleteHandler&) = delete;
  CompleteHandler& operator=(const CompleteHandler&) = delete;

  MOCK_METHOD1(Complete, void(int result));
};

// Creates a CastMessage proto with the bare minimum required fields set.
CastMessage CreateCastMessage() {
  CastMessage output;
  output.set_protocol_version(CastMessage::CASTV2_1_0);
  output.set_namespace_("x");
  output.set_source_id("source");
  output.set_destination_id("destination");
  output.set_payload_type(CastMessage::STRING);
  output.set_payload_utf8("payload");
  return output;
}

// FIFO queue of completion callbacks. Outstanding write operations are
// Push()ed into the queue. Callback completion is simulated by invoking
// Pop() in the same order as Push().
class CompletionQueue {
 public:
  CompletionQueue() = default;

  CompletionQueue(const CompletionQueue&) = delete;
  CompletionQueue& operator=(const CompletionQueue&) = delete;

  ~CompletionQueue() { CHECK_EQ(0u, cb_queue_.size()); }

  // Enqueues a pending completion callback.
  void Push(net::CompletionOnceCallback cb) { cb_queue_.push(std::move(cb)); }
  // Runs the next callback and removes it from the queue.
  void Pop(int rv) {
    CHECK_GT(cb_queue_.size(), 0u);
    std::move(cb_queue_.front()).Run(rv);
    cb_queue_.pop();
  }

 private:
  base::queue<net::CompletionOnceCallback> cb_queue_;
};

// GMock action that reads data from an IOBuffer and writes it to a string
// variable.
//
//   buf_idx (template parameter 0): 0-based index of the net::IOBuffer
//                                   in the function mock arg list.
//   size_idx (template parameter 1): 0-based index of the byte count arg.
//   str: pointer to the string which will receive data from the buffer.
ACTION_TEMPLATE(ReadBufferToString,
                HAS_2_TEMPLATE_PARAMS(int, buf_idx, int, size_idx),
                AND_1_VALUE_PARAMS(str)) {
  str->assign(testing::get<buf_idx>(args)->data(),
              testing::get<size_idx>(args));
}

// GMock action that writes data from a string to an IOBuffer.
//
//   buf_idx (template parameter 0): 0-based index of the IOBuffer arg.
//   str: the string containing data to be written to the IOBuffer.
ACTION_TEMPLATE(FillBufferFromString,
                HAS_1_TEMPLATE_PARAMS(int, buf_idx),
                AND_1_VALUE_PARAMS(str)) {
  memcpy(testing::get<buf_idx>(args)->data(), str.data(), str.size());
}

// GMock action that enqueues a write completion callback in a queue.
//
//   buf_idx (template parameter 0): 0-based index of the CompletionCallback.
//   completion_queue: a pointer to the CompletionQueue.
ACTION_TEMPLATE(EnqueueCallback,
                HAS_1_TEMPLATE_PARAMS(int, cb_idx),
                AND_1_VALUE_PARAMS(completion_queue)) {
  completion_queue->Push(std::move(testing::get<cb_idx>(args)));
}

}  // namespace

class MockSocket : public cast_channel::CastTransportImpl::Channel {
 public:
  void Read(net::IOBuffer* buffer,
            int bytes,
            net::CompletionOnceCallback callback) override {
    Read_(buffer, bytes, callback);
  }

  void Write(net::IOBuffer* buffer,
             int bytes,
             net::CompletionOnceCallback callback) override {
    Write_(buffer, bytes, callback);
  }

  MOCK_METHOD3(Read_,
               void(net::IOBuffer* buf,
                    int buf_len,
                    net::CompletionOnceCallback& callback));

  MOCK_METHOD3(Write_,
               void(net::IOBuffer* buf,
                    int buf_len,
                    net::CompletionOnceCallback& callback));
};

class CastTransportTest : public testing::Test {
 public:
  CastTransportTest() : logger_(new Logger()) {
    delegate_ = new MockCastTransportDelegate;
    transport_ = std::make_unique<CastTransportImpl>(
        &mock_socket_, kChannelId, CreateIPEndPointForTest(), logger_);
    transport_->SetReadDelegate(base::WrapUnique(delegate_.get()));
  }
  ~CastTransportTest() override = default;

 protected:
  // Runs all pending tasks in the message loop.
  void RunPendingTasks() {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  raw_ptr<MockCastTransportDelegate, DanglingUntriaged> delegate_;
  MockSocket mock_socket_;
  scoped_refptr<Logger> logger_;
  std::unique_ptr<CastTransport> transport_;
};

// ----------------------------------------------------------------------------
// Asynchronous write tests
TEST_F(CastTransportTest, TestFullWriteAsync) {
  CompletionQueue socket_cbs;
  CompleteHandler write_handler;
  std::string output;

  CastMessage message = CreateCastMessage();
  std::string serialized_message;
  EXPECT_TRUE(MessageFramer::Serialize(message, &serialized_message));

  EXPECT_CALL(mock_socket_, Write_(NotNull(), serialized_message.size(), _))
      .WillOnce(DoAll(ReadBufferToString<0, 1>(&output),
                      EnqueueCallback<2>(&socket_cbs)));
  EXPECT_CALL(write_handler, Complete(net::OK));
  transport_->SendMessage(message,
                          base::BindOnce(&CompleteHandler::Complete,
                                         base::Unretained(&write_handler)));
  RunPendingTasks();
  socket_cbs.Pop(serialized_message.size());
  RunPendingTasks();
  EXPECT_EQ(serialized_message, output);
}

TEST_F(CastTransportTest, TestPartialWritesAsync) {
  InSequence seq;
  CompletionQueue socket_cbs;
  CompleteHandler write_handler;
  std::string output;

  CastMessage message = CreateCastMessage();
  std::string serialized_message;
  EXPECT_TRUE(MessageFramer::Serialize(message, &serialized_message));

  // Only one byte is written.
  EXPECT_CALL(mock_socket_,
              Write_(NotNull(), static_cast<int>(serialized_message.size()), _))
      .WillOnce(DoAll(ReadBufferToString<0, 1>(&output),
                      EnqueueCallback<2>(&socket_cbs)));
  // Remainder of bytes are written.
  EXPECT_CALL(
      mock_socket_,
      Write_(NotNull(), static_cast<int>(serialized_message.size() - 1), _))
      .WillOnce(DoAll(ReadBufferToString<0, 1>(&output),
                      EnqueueCallback<2>(&socket_cbs)));

  transport_->SendMessage(message,
                          base::BindOnce(&CompleteHandler::Complete,
                                         base::Unretained(&write_handler)));
  RunPendingTasks();
  EXPECT_EQ(serialized_message, output);
  socket_cbs.Pop(1);
  RunPendingTasks();

  EXPECT_CALL(write_handler, Complete(net::OK));
  socket_cbs.Pop(serialized_message.size() - 1);
  RunPendingTasks();
  EXPECT_EQ(serialized_message.substr(1, serialized_message.size() - 1),
            output);
}

TEST_F(CastTransportTest, TestWriteFailureAsync) {
  CompletionQueue socket_cbs;
  CompleteHandler write_handler;
  CastMessage message = CreateCastMessage();
  EXPECT_CALL(mock_socket_, Write_(NotNull(), _, _))
      .WillOnce(EnqueueCallback<2>(&socket_cbs));
  EXPECT_CALL(write_handler, Complete(net::ERR_FAILED));
  EXPECT_CALL(*delegate_, OnError(ChannelError::CAST_SOCKET_ERROR));
  transport_->SendMessage(message,
                          base::BindOnce(&CompleteHandler::Complete,
                                         base::Unretained(&write_handler)));
  RunPendingTasks();
  socket_cbs.Pop(net::ERR_CONNECTION_RESET);
  RunPendingTasks();
  EXPECT_EQ(ChannelEvent::SOCKET_WRITE,
            logger_->GetLastError(kChannelId).channel_event);
  EXPECT_EQ(net::ERR_CONNECTION_RESET,
            logger_->GetLastError(kChannelId).net_return_value);
}

// ----------------------------------------------------------------------------
// Asynchronous read tests
TEST_F(CastTransportTest, TestFullReadAsync) {
  InSequence s;
  CompletionQueue socket_cbs;

  CastMessage message = CreateCastMessage();
  std::string serialized_message;
  EXPECT_TRUE(MessageFramer::Serialize(message, &serialized_message));
  EXPECT_CALL(*delegate_, Start());

  // Read bytes [0, 3].
  EXPECT_CALL(mock_socket_,
              Read_(NotNull(), sizeof(MessageFramer::MessageHeader), _))
      .WillOnce(DoAll(FillBufferFromString<0>(serialized_message),
                      EnqueueCallback<2>(&socket_cbs)));

  // Read bytes [4, n].
  EXPECT_CALL(mock_socket_, Read_(NotNull(),
                                  serialized_message.size() -
                                      sizeof(MessageFramer::MessageHeader),
                                  _))
      .WillOnce(DoAll(FillBufferFromString<0>(serialized_message.substr(
                          sizeof(MessageFramer::MessageHeader),
                          serialized_message.size() -
                              sizeof(MessageFramer::MessageHeader))),
                      EnqueueCallback<2>(&socket_cbs)))
      .RetiresOnSaturation();

  EXPECT_CALL(*delegate_, OnMessage(EqualsProto(message)));
  EXPECT_CALL(mock_socket_,
              Read_(NotNull(), sizeof(MessageFramer::MessageHeader), _));
  transport_->Start();
  RunPendingTasks();
  socket_cbs.Pop(sizeof(MessageFramer::MessageHeader));
  socket_cbs.Pop(serialized_message.size() -
                 sizeof(MessageFramer::MessageHeader));
  RunPendingTasks();
}

TEST_F(CastTransportTest, TestPartialReadAsync) {
  InSequence s;
  CompletionQueue socket_cbs;

  CastMessage message = CreateCastMessage();
  std::string serialized_message;
  EXPECT_TRUE(MessageFramer::Serialize(message, &serialized_message));

  EXPECT_CALL(*delegate_, Start());

  // Read bytes [0, 3].
  EXPECT_CALL(mock_socket_,
              Read_(NotNull(), sizeof(MessageFramer::MessageHeader), _))
      .WillOnce(DoAll(FillBufferFromString<0>(serialized_message),
                      EnqueueCallback<2>(&socket_cbs)))
      .RetiresOnSaturation();
  // Read bytes [4, n-1].
  EXPECT_CALL(mock_socket_, Read_(NotNull(),
                                  serialized_message.size() -
                                      sizeof(MessageFramer::MessageHeader),
                                  _))
      .WillOnce(DoAll(FillBufferFromString<0>(serialized_message.substr(
                          sizeof(MessageFramer::MessageHeader),
                          serialized_message.size() -
                              sizeof(MessageFramer::MessageHeader) - 1)),
                      EnqueueCallback<2>(&socket_cbs)))
      .RetiresOnSaturation();
  // Read final byte.
  EXPECT_CALL(mock_socket_, Read_(NotNull(), 1, _))
      .WillOnce(DoAll(FillBufferFromString<0>(serialized_message.substr(
                          serialized_message.size() - 1, 1)),
                      EnqueueCallback<2>(&socket_cbs)))
      .RetiresOnSaturation();
  EXPECT_CALL(*delegate_, OnMessage(EqualsProto(message)));
  transport_->Start();
  socket_cbs.Pop(sizeof(MessageFramer::MessageHeader));
  socket_cbs.Pop(serialized_message.size() -
                 sizeof(MessageFramer::MessageHeader) - 1);
  EXPECT_CALL(mock_socket_,
              Read_(NotNull(), sizeof(MessageFramer::MessageHeader), _));
  socket_cbs.Pop(1);
}

TEST_F(CastTransportTest, TestReadErrorInHeaderAsync) {
  CompletionQueue socket_cbs;

  CastMessage message = CreateCastMessage();
  std::string serialized_message;
  EXPECT_TRUE(MessageFramer::Serialize(message, &serialized_message));

  EXPECT_CALL(*delegate_, Start());

  // Read bytes [0, 3].
  EXPECT_CALL(mock_socket_,
              Read_(NotNull(), sizeof(MessageFramer::MessageHeader), _))
      .WillOnce(DoAll(FillBufferFromString<0>(serialized_message),
                      EnqueueCallback<2>(&socket_cbs)))
      .RetiresOnSaturation();

  EXPECT_CALL(*delegate_, OnError(ChannelError::CAST_SOCKET_ERROR));
  transport_->Start();
  // Header read failure.
  socket_cbs.Pop(net::ERR_CONNECTION_RESET);
  EXPECT_EQ(ChannelEvent::SOCKET_READ,
            logger_->GetLastError(kChannelId).channel_event);
  EXPECT_EQ(net::ERR_CONNECTION_RESET,
            logger_->GetLastError(kChannelId).net_return_value);
}

TEST_F(CastTransportTest, TestReadErrorInBodyAsync) {
  CompletionQueue socket_cbs;

  CastMessage message = CreateCastMessage();
  std::string serialized_message;
  EXPECT_TRUE(MessageFramer::Serialize(message, &serialized_message));

  EXPECT_CALL(*delegate_, Start());

  // Read bytes [0, 3].
  EXPECT_CALL(mock_socket_,
              Read_(NotNull(), sizeof(MessageFramer::MessageHeader), _))
      .WillOnce(DoAll(FillBufferFromString<0>(serialized_message),
                      EnqueueCallback<2>(&socket_cbs)))
      .RetiresOnSaturation();
  // Read bytes [4, n-1].
  EXPECT_CALL(mock_socket_, Read_(NotNull(),
                                  serialized_message.size() -
                                      sizeof(MessageFramer::MessageHeader),
                                  _))
      .WillOnce(DoAll(FillBufferFromString<0>(serialized_message.substr(
                          sizeof(MessageFramer::MessageHeader),
                          serialized_message.size() -
                              sizeof(MessageFramer::MessageHeader) - 1)),
                      EnqueueCallback<2>(&socket_cbs)))
      .RetiresOnSaturation();
  EXPECT_CALL(*delegate_, OnError(ChannelError::CAST_SOCKET_ERROR));
  transport_->Start();
  // Header read is OK.
  socket_cbs.Pop(sizeof(MessageFramer::MessageHeader));
  // Body read fails.
  socket_cbs.Pop(net::ERR_CONNECTION_RESET);
  EXPECT_EQ(ChannelEvent::SOCKET_READ,
            logger_->GetLastError(kChannelId).channel_event);
  EXPECT_EQ(net::ERR_CONNECTION_RESET,
            logger_->GetLastError(kChannelId).net_return_value);
}

TEST_F(CastTransportTest, TestReadCorruptedMessageAsync) {
  CompletionQueue socket_cbs;

  CastMessage message = CreateCastMessage();
  std::string serialized_message;
  EXPECT_TRUE(MessageFramer::Serialize(message, &serialized_message));

  // Corrupt the serialized message body(set it to X's).
  for (size_t i = sizeof(MessageFramer::MessageHeader);
       i < serialized_message.size(); ++i) {
    serialized_message[i] = 'x';
  }

  EXPECT_CALL(*delegate_, Start());
  // Read bytes [0, 3].
  EXPECT_CALL(mock_socket_,
              Read_(NotNull(), sizeof(MessageFramer::MessageHeader), _))
      .WillOnce(DoAll(FillBufferFromString<0>(serialized_message),
                      EnqueueCallback<2>(&socket_cbs)))
      .RetiresOnSaturation();
  // Read bytes [4, n].
  EXPECT_CALL(mock_socket_, Read_(NotNull(),
                                  serialized_message.size() -
                                      sizeof(MessageFramer::MessageHeader),
                                  _))
      .WillOnce(DoAll(FillBufferFromString<0>(serialized_message.substr(
                          sizeof(MessageFramer::MessageHeader),
                          serialized_message.size() -
                              sizeof(MessageFramer::MessageHeader) - 1)),
                      EnqueueCallback<2>(&socket_cbs)))
      .RetiresOnSaturation();

  EXPECT_CALL(*delegate_, OnError(ChannelError::INVALID_MESSAGE));
  transport_->Start();
  socket_cbs.Pop(sizeof(MessageFramer::MessageHeader));
  socket_cbs.Pop(serialized_message.size() -
                 sizeof(MessageFramer::MessageHeader));
}

}  // namespace cast_channel
