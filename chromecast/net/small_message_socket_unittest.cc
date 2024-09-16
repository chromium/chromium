// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/net/small_message_socket.h"

#include <memory>
#include <string>
#include <vector>

#include "base/big_endian.h"
#include "base/test/task_environment.h"
#include "chromecast/net/fake_stream_socket.h"
#include "chromecast/net/io_buffer_pool.h"
#include "net/base/io_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {

namespace {

constexpr size_t kDefaultMessageSize = 256;
constexpr size_t kLargeMessageSize = 100000;

const char kIpAddress1[] = "192.168.0.1";
const uint16_t kPort1 = 10001;
const char kIpAddress2[] = "192.168.0.2";
const uint16_t kPort2 = 10002;

net::IPAddress IpLiteralToIpAddress(const std::string& ip_literal) {
  net::IPAddress ip_address;
  CHECK(ip_address.AssignFromIPLiteral(ip_literal));
  return ip_address;
}

void SetData(base::span<char> buffer) {
  for (size_t i = 0u; i < buffer.size(); ++i) {
    buffer[i] = static_cast<char>(i);
  }
}

void CheckData(base::span<const char> buffer) {
  for (size_t i = 0u; i < buffer.size(); ++i) {
    EXPECT_EQ(buffer[i], static_cast<char>(i));
  }
}

class TestSocket : public SmallMessageSocket::Delegate {
 public:
  explicit TestSocket(std::unique_ptr<net::Socket> socket)
      : socket_(this, std::move(socket)) {}

  ~TestSocket() override = default;

  void UseBufferPool() {
    buffer_pool_ = base::MakeRefCounted<IOBufferPool>(kDefaultMessageSize +
                                                      sizeof(uint16_t));
    socket_.UseBufferPool(buffer_pool_);
  }
  void SwapPoolUse(bool swap) { swap_pool_use_ = swap; }

  void* PrepareSend(int message_size) {
    return socket_.PrepareSend(message_size);
  }
  void Send() { socket_.Send(); }
  bool SendBuffer(scoped_refptr<net::IOBuffer> data, int size) {
    return socket_.SendBuffer(std::move(data), size);
  }

  void SendData(size_t size) {
    size_t data_offset = SmallMessageSocket::SizeDataBytes(size);
    auto buffer =
        base::MakeRefCounted<net::IOBufferWithSize>(data_offset + size);
    ASSERT_EQ(buffer->span().size(), size + data_offset);
    size_t written = SmallMessageSocket::WriteSizeData(
        base::as_writable_bytes(buffer->span()), size);
    ASSERT_EQ(written, data_offset);
    SetData(base::as_writable_chars(buffer->span().subspan(data_offset)));
    SendBuffer(std::move(buffer), data_offset + size);
  }

  void ReceiveMessages() { socket_.ReceiveMessages(); }

  size_t last_message_size() const {
    DCHECK(!message_history_.empty());
    return message_history_[message_history_.size() - 1];
  }

  const std::vector<size_t>& message_history() const {
    return message_history_;
  }

  IOBufferPool* buffer_pool() const { return buffer_pool_.get(); }
  SmallMessageSocket* socket() { return &socket_; }

 private:
  void OnError(int error) override { NOTREACHED(); }

  bool OnMessage(char* data, size_t size) override {
    message_history_.push_back(size);
    CheckData(
        // TODO(crbug.com/40284755): OnMessage() should receive a span.
        UNSAFE_TODO(base::span(data, size)));
    if (swap_pool_use_) {
      UseBufferPool();
    }
    return true;
  }

  bool OnMessageBuffer(scoped_refptr<net::IOBuffer> buffer,
                       size_t size) override {
    size_t data_offset;
    size_t message_size;
    bool read_ok = SmallMessageSocket::ReadSize(buffer->data(), size,
                                                data_offset, message_size);
    EXPECT_TRUE(read_ok);
    if (!read_ok) {
      return false;
    }
    EXPECT_EQ(message_size, size - data_offset);
    message_history_.push_back(message_size);
    CheckData(
        base::as_chars(buffer->span().subspan(data_offset, message_size)));
    if (swap_pool_use_) {
      socket_.RemoveBufferPool();
      buffer_pool_ = nullptr;
    }
    return true;
  }

  SmallMessageSocket socket_;
  std::vector<size_t> message_history_;
  scoped_refptr<IOBufferPool> buffer_pool_;
  bool swap_pool_use_ = false;
};

}  // namespace

class SmallMessageSocketTest : public ::testing::Test {
 public:
  SmallMessageSocketTest() {
    auto fake1 = std::make_unique<FakeStreamSocket>(
        net::IPEndPoint(IpLiteralToIpAddress(kIpAddress1), kPort1));
    auto fake2 = std::make_unique<FakeStreamSocket>(
        net::IPEndPoint(IpLiteralToIpAddress(kIpAddress2), kPort2));
    fake1->SetPeer(fake2.get());
    fake2->SetPeer(fake1.get());
    fake1->SetBadSenderMode(true);
    fake2->SetBadSenderMode(true);

    socket_1_ = std::make_unique<TestSocket>(std::move(fake1));
    socket_2_ = std::make_unique<TestSocket>(std::move(fake2));
  }

  ~SmallMessageSocketTest() override = default;

 protected:
  static scoped_refptr<SmallMessageSocket::BufferWrapper>
  CreateBufferWrapper() {
    return base::MakeRefCounted<SmallMessageSocket::BufferWrapper>();
  }

  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<TestSocket> socket_1_;
  std::unique_ptr<TestSocket> socket_2_;
};

TEST_F(SmallMessageSocketTest, SendAndReceive) {
  socket_2_->ReceiveMessages();
  socket_1_->SendData(kDefaultMessageSize);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(socket_2_->last_message_size(), kDefaultMessageSize);
}

TEST_F(SmallMessageSocketTest, SendAndReceiveLarge) {
  size_t message_size = kLargeMessageSize;
  socket_2_->ReceiveMessages();
  for (int i = 0; i < 5; ++i) {
    socket_1_->SendData(message_size);
    task_environment_.RunUntilIdle();
    EXPECT_EQ(socket_2_->last_message_size(), message_size);

    socket_1_->SendData(kDefaultMessageSize);
    task_environment_.RunUntilIdle();
    EXPECT_EQ(socket_2_->last_message_size(), kDefaultMessageSize);

    --message_size;
  }
}

TEST_F(SmallMessageSocketTest, PrepareSendAndReceive) {
  char* buffer =
      static_cast<char*>(socket_1_->PrepareSend(kDefaultMessageSize));
  SetData(
      // TODO(crbug.com/40284755): PrepareSend() should return a span.
      UNSAFE_TODO(base::span(buffer, kDefaultMessageSize)));
  socket_2_->ReceiveMessages();
  socket_1_->Send();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(socket_2_->last_message_size(), kDefaultMessageSize);
}

TEST_F(SmallMessageSocketTest, MultipleMessages) {
  char* buffer = static_cast<char*>(socket_1_->PrepareSend(kLargeMessageSize));

  SetData(
      // TODO(crbug.com/40284755): PrepareSend() should return a span.
      UNSAFE_TODO(base::span(buffer, kLargeMessageSize)));
  socket_1_->Send();
  task_environment_.RunUntilIdle();

  buffer =
      static_cast<char*>(socket_1_->PrepareSend(kDefaultMessageSize * 2 + 1));
  SetData(
      // TODO(crbug.com/40284755): PrepareSend() should return a span.
      UNSAFE_TODO(base::span(buffer, kDefaultMessageSize * 2 + 1)));
  socket_1_->Send();
  task_environment_.RunUntilIdle();

  buffer = static_cast<char*>(socket_1_->PrepareSend(kDefaultMessageSize - 5));
  SetData(
      // TODO(crbug.com/40284755): PrepareSend() should return a span.
      UNSAFE_TODO(base::span(buffer, kDefaultMessageSize - 5)));
  socket_1_->Send();
  task_environment_.RunUntilIdle();

  socket_2_->ReceiveMessages();
  task_environment_.RunUntilIdle();

  ASSERT_EQ(socket_2_->message_history().size(), 3u);
  EXPECT_EQ(socket_2_->message_history()[0], kLargeMessageSize);
  EXPECT_EQ(socket_2_->message_history()[1], kDefaultMessageSize * 2 + 1);
  EXPECT_EQ(socket_2_->message_history()[2], kDefaultMessageSize - 5);
}

TEST_F(SmallMessageSocketTest, BufferSendAndReceive) {
  socket_1_->UseBufferPool();
  socket_2_->UseBufferPool();
  socket_2_->ReceiveMessages();
  socket_1_->SendData(kDefaultMessageSize);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(socket_2_->last_message_size(), kDefaultMessageSize);
  EXPECT_GT(socket_2_->buffer_pool()->NumAllocatedForTesting(), 0u);
  EXPECT_GT(socket_2_->buffer_pool()->NumFreeForTesting(), 0u);
}

TEST_F(SmallMessageSocketTest, SendLargerThanPoolBufferSize) {
  socket_1_->UseBufferPool();
  socket_2_->UseBufferPool();
  char* buffer = static_cast<char*>(socket_1_->PrepareSend(kLargeMessageSize));
  SetData(
      // TODO(crbug.com/40284755): PrepareSend() should return a span.
      UNSAFE_TODO(base::span(buffer, kLargeMessageSize)));
  socket_2_->ReceiveMessages();
  socket_1_->Send();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(socket_2_->last_message_size(), kLargeMessageSize);
}

TEST_F(SmallMessageSocketTest, BufferMultipleMessages) {
  socket_1_->UseBufferPool();
  socket_2_->UseBufferPool();
  char* buffer =
      static_cast<char*>(socket_1_->PrepareSend(kDefaultMessageSize - 1));
  SetData(
      // TODO(crbug.com/40284755): PrepareSend() should return a span.
      UNSAFE_TODO(base::span(buffer, kDefaultMessageSize - 1)));
  socket_1_->Send();
  task_environment_.RunUntilIdle();

  buffer = static_cast<char*>(socket_1_->PrepareSend(kLargeMessageSize));
  SetData(
      // TODO(crbug.com/40284755): PrepareSend() should return a span.
      UNSAFE_TODO(base::span(buffer, kLargeMessageSize)));
  socket_1_->Send();
  task_environment_.RunUntilIdle();

  buffer = static_cast<char*>(socket_1_->PrepareSend(kDefaultMessageSize - 5));
  SetData(
      // TODO(crbug.com/40284755): PrepareSend() should return a span.
      UNSAFE_TODO(base::span(buffer, kDefaultMessageSize - 5)));
  socket_1_->Send();
  task_environment_.RunUntilIdle();

  buffer = static_cast<char*>(socket_1_->PrepareSend(kDefaultMessageSize));
  SetData(
      // TODO(crbug.com/40284755): PrepareSend() should return a span.
      UNSAFE_TODO(base::span(buffer, kDefaultMessageSize)));
  socket_1_->Send();
  task_environment_.RunUntilIdle();

  socket_2_->ReceiveMessages();
  task_environment_.RunUntilIdle();

  ASSERT_EQ(socket_2_->message_history().size(), 4u);
  EXPECT_EQ(socket_2_->message_history()[0], kDefaultMessageSize - 1);
  EXPECT_EQ(socket_2_->message_history()[1], kLargeMessageSize);
  EXPECT_EQ(socket_2_->message_history()[2], kDefaultMessageSize - 5);
  EXPECT_EQ(socket_2_->message_history()[3], kDefaultMessageSize);
}

TEST_F(SmallMessageSocketTest, SwapPoolUse) {
  socket_2_->SwapPoolUse(true);
  char* buffer =
      static_cast<char*>(socket_1_->PrepareSend(kDefaultMessageSize * 2 + 1));
  SetData(
      // TODO(crbug.com/40284755): PrepareSend() should return a span.
      UNSAFE_TODO(base::span(buffer, kDefaultMessageSize * 2 + 1)));
  socket_1_->Send();
  task_environment_.RunUntilIdle();

  buffer = static_cast<char*>(socket_1_->PrepareSend(kDefaultMessageSize - 5));
  SetData(
      // TODO(crbug.com/40284755): PrepareSend() should return a span.
      UNSAFE_TODO(base::span(buffer, kDefaultMessageSize - 5)));
  socket_1_->Send();
  task_environment_.RunUntilIdle();

  buffer = static_cast<char*>(socket_1_->PrepareSend(kDefaultMessageSize));
  SetData(
      // TODO(crbug.com/40284755): PrepareSend() should return a span.
      UNSAFE_TODO(base::span(buffer, kDefaultMessageSize)));
  socket_1_->Send();
  task_environment_.RunUntilIdle();

  socket_2_->ReceiveMessages();
  task_environment_.RunUntilIdle();

  ASSERT_EQ(socket_2_->message_history().size(), 3u);
  EXPECT_EQ(socket_2_->message_history()[0], kDefaultMessageSize * 2 + 1);
  EXPECT_EQ(socket_2_->message_history()[1], kDefaultMessageSize - 5);
  EXPECT_EQ(socket_2_->message_history()[2], kDefaultMessageSize);
}

TEST_F(SmallMessageSocketTest, BufferWrapper) {
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(10);
  base::span<const char> buffer_data = base::as_chars(buffer->span());
  auto wrapper = CreateBufferWrapper();
  wrapper->SetUnderlyingBuffer(std::move(buffer), 9);
  EXPECT_EQ(wrapper->data(), &buffer_data[0u]);
  EXPECT_EQ(wrapper->size(), 9);
  EXPECT_EQ(wrapper->capacity(), 9u);
  EXPECT_EQ(wrapper->used(), 0u);
  EXPECT_EQ(wrapper->StartOfBuffer(), &buffer_data[0u]);

  wrapper->DidConsume(3);
  EXPECT_EQ(wrapper->data(), &buffer_data[3u]);
  EXPECT_EQ(wrapper->size(), 6);
  EXPECT_EQ(wrapper->capacity(), 9u);
  EXPECT_EQ(wrapper->used(), 3u);
  EXPECT_EQ(wrapper->StartOfBuffer(), &buffer_data[0u]);
}

}  // namespace chromecast
