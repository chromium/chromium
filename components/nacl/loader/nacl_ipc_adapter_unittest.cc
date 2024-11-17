// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/nacl/loader/nacl_ipc_adapter.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/platform_thread.h"
#include "base/threading/simple_thread.h"
#include "ipc/ipc_test_sink.h"
#include "native_client/src/public/nacl_desc_custom.h"
#include "native_client/src/trusted/service_runtime/include/sys/fcntl.h"
#include "ppapi/c/ppb_file_io.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class NaClIPCAdapterTest : public testing::Test {
 public:
  NaClIPCAdapterTest() = default;

  // testing::Test implementation.
  void SetUp() override {
    sink_ = new IPC::TestSink;

    // Takes ownership of the sink_ pointer. Note we provide the current message
    // loop instead of using a real IO thread. This should work OK since we do
    // not need real IPC for the tests.
    adapter_ = new NaClIPCAdapter(
        std::unique_ptr<IPC::Channel>(sink_),
        base::SingleThreadTaskRunner::GetCurrentDefault().get());
  }
  void TearDown() override {
    sink_ = nullptr;  // This pointer is actually owned by the IPCAdapter.
    adapter_.reset();
    // The adapter destructor has to post a task to destroy the Channel on the
    // IO thread. For the purposes of the test, we just need to make sure that
    // task gets run, or it will appear as a leak.
    base::RunLoop().RunUntilIdle();
  }

 protected:
  int BlockingReceive(void* buf, size_t buf_size) {
    NaClImcMsgIoVec iov = {buf, buf_size};
    NaClImcTypedMsgHdr msg = {&iov, 1};
    return adapter_->BlockingReceive(&msg);
  }

  int Send(void* buf, size_t buf_size) {
    NaClImcMsgIoVec iov = {buf, buf_size};
    NaClImcTypedMsgHdr msg = {&iov, 1};
    return adapter_->Send(&msg);
  }

  base::test::SingleThreadTaskEnvironment task_environment_;

  scoped_refptr<NaClIPCAdapter> adapter_;

  // Messages sent from nacl to the adapter end up here. Note that we create
  // this pointer and pass ownership of it to the IPC adapter, who will keep
  // it alive as long as the adapter is alive. This means that when the
  // adapter goes away, this pointer will become invalid.
  //
  // In real life the adapter needs to take ownership so the channel can be
  // destroyed on the right thread.
  raw_ptr<IPC::TestSink> sink_;
};

}  // namespace

// Tests a simple message getting rewritten sent from native code to NaCl.
TEST_F(NaClIPCAdapterTest, SimpleReceiveRewriting) {
  int routing_id = 0x89898989;
  uint32_t type = 0x55555555;
  IPC::Message input(routing_id, type, IPC::Message::PRIORITY_NORMAL);
  uint32_t flags = input.flags();

  int value = 0x12345678;
  input.WriteInt(value);
  adapter_->OnMessageReceived(input);

  // Buffer just need to be big enough for our message with one int.
  const int kBufSize = 64;
  char buf[kBufSize];

  int bytes_read = BlockingReceive(buf, kBufSize);
  EXPECT_EQ(sizeof(NaClIPCAdapter::NaClMessageHeader) + sizeof(int),
            static_cast<size_t>(bytes_read));

  const NaClIPCAdapter::NaClMessageHeader* output_header =
      reinterpret_cast<const NaClIPCAdapter::NaClMessageHeader*>(buf);
  EXPECT_EQ(sizeof(int), output_header->payload_size);
  EXPECT_EQ(routing_id, output_header->routing);
  EXPECT_EQ(type, output_header->type);
  EXPECT_EQ(flags, output_header->flags);
  EXPECT_EQ(0u, output_header->num_fds);
  EXPECT_EQ(0u, output_header->pad);

  // Validate the payload.
  EXPECT_EQ(value,
            *reinterpret_cast<const int*>(&buf[
                sizeof(NaClIPCAdapter::NaClMessageHeader)]));
}

// Tests a simple message getting rewritten sent from NaCl to native code.
TEST_F(NaClIPCAdapterTest, SendRewriting) {
  int routing_id = 0x89898989;
  uint32_t type = 0x55555555;
  int value = 0x12345678;

  // Send a message with one int inside it.
  const int buf_size = sizeof(NaClIPCAdapter::NaClMessageHeader) + sizeof(int);
  char buf[buf_size] = {0};

  NaClIPCAdapter::NaClMessageHeader* header =
      reinterpret_cast<NaClIPCAdapter::NaClMessageHeader*>(buf);
  header->payload_size = sizeof(int);
  header->routing = routing_id;
  header->type = type;
  header->flags = 0;
  header->num_fds = 0;
  *reinterpret_cast<int*>(
      &buf[sizeof(NaClIPCAdapter::NaClMessageHeader)]) = value;

  int result = Send(buf, buf_size);
  EXPECT_EQ(buf_size, result);

  // Check that the message came out the other end in the test sink
  // (messages are posted, so we have to pump).
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, sink_->message_count());
  const IPC::Message* msg = sink_->GetMessageAt(0);

  EXPECT_EQ(sizeof(int), msg->payload_size());
  EXPECT_EQ(header->routing, msg->routing_id());
  EXPECT_EQ(header->type, msg->type());

  // Now test the partial send case. We should be able to break the message
  // into two parts and it should still work.
  sink_->ClearMessages();
  int first_chunk_size = 7;
  result = Send(buf, first_chunk_size);
  EXPECT_EQ(first_chunk_size, result);

  // First partial send should not have made any messages.
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(0u, sink_->message_count());

  // Second partial send should do the same.
  int second_chunk_size = 2;
  result = Send(&buf[first_chunk_size], second_chunk_size);
  EXPECT_EQ(second_chunk_size, result);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(0u, sink_->message_count());

  // Send the rest of the message in a third chunk.
  int third_chunk_size = buf_size - first_chunk_size - second_chunk_size;
  result = Send(&buf[first_chunk_size + second_chunk_size],
                          third_chunk_size);
  EXPECT_EQ(third_chunk_size, result);

  // Last send should have generated one message.
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, sink_->message_count());
  msg = sink_->GetMessageAt(0);
  EXPECT_EQ(sizeof(int), msg->payload_size());
  EXPECT_EQ(header->routing, msg->routing_id());
  EXPECT_EQ(header->type, msg->type());
}

// Tests when a buffer is too small to receive the entire message.
TEST_F(NaClIPCAdapterTest, PartialReceive) {
  int routing_id_1 = 0x89898989;
  uint32_t type_1 = 0x55555555;
  IPC::Message input_1(routing_id_1, type_1, IPC::Message::PRIORITY_NORMAL);
  int value_1 = 0x12121212;
  input_1.WriteInt(value_1);
  adapter_->OnMessageReceived(input_1);

  int routing_id_2 = 0x90909090;
  uint32_t type_2 = 0x66666666;
  IPC::Message input_2(routing_id_2, type_2, IPC::Message::PRIORITY_NORMAL);
  int value_2 = 0x23232323;
  input_2.WriteInt(value_2);
  adapter_->OnMessageReceived(input_2);

  const int kBufSize = 64;
  char buf[kBufSize];

  // Read part of the first message.
  int bytes_requested = 7;
  int bytes_read = BlockingReceive(buf, bytes_requested);
  ASSERT_EQ(bytes_requested, bytes_read);

  // Read the rest, this should give us the rest of the first message only.
  bytes_read += BlockingReceive(&buf[bytes_requested],
                                        kBufSize - bytes_requested);
  EXPECT_EQ(sizeof(NaClIPCAdapter::NaClMessageHeader) + sizeof(int),
            static_cast<size_t>(bytes_read));

  // Make sure we got the right message.
  const NaClIPCAdapter::NaClMessageHeader* output_header =
      reinterpret_cast<const NaClIPCAdapter::NaClMessageHeader*>(buf);
  EXPECT_EQ(sizeof(int), output_header->payload_size);
  EXPECT_EQ(routing_id_1, output_header->routing);
  EXPECT_EQ(type_1, output_header->type);

  // Read the second message to make sure we went on to it.
  bytes_read = BlockingReceive(buf, kBufSize);
  EXPECT_EQ(sizeof(NaClIPCAdapter::NaClMessageHeader) + sizeof(int),
            static_cast<size_t>(bytes_read));
  output_header =
      reinterpret_cast<const NaClIPCAdapter::NaClMessageHeader*>(buf);
  EXPECT_EQ(sizeof(int), output_header->payload_size);
  EXPECT_EQ(routing_id_2, output_header->routing);
  EXPECT_EQ(type_2, output_header->type);
}

// Tests sending messages that are too large. We test sends that are too
// small implicitly here and in the success case because in that case it
// succeeds and buffers the data.
TEST_F(NaClIPCAdapterTest, SendOverflow) {
  int routing_id = 0x89898989;
  uint32_t type = 0x55555555;
  int value = 0x12345678;

  // Make a message with one int inside it. Reserve some extra space so
  // we can test what happens when we send too much data.
  const int buf_size = sizeof(NaClIPCAdapter::NaClMessageHeader) + sizeof(int);
  const int big_buf_size = buf_size + 4;
  char buf[big_buf_size] = {0};

  NaClIPCAdapter::NaClMessageHeader* header =
      reinterpret_cast<NaClIPCAdapter::NaClMessageHeader*>(buf);
  header->payload_size = sizeof(int);
  header->routing = routing_id;
  header->type = type;
  header->flags = 0;
  header->num_fds = 0;
  *reinterpret_cast<int*>(
      &buf[sizeof(NaClIPCAdapter::NaClMessageHeader)]) = value;

  // Send too much data and make sure that the send fails.
  int result = Send(buf, big_buf_size);
  EXPECT_EQ(-1, result);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(0u, sink_->message_count());

  // Send too much data in two chunks and make sure that the send fails.
  int first_chunk_size = 7;
  result = Send(buf, first_chunk_size);
  EXPECT_EQ(first_chunk_size, result);

  // First partial send should not have made any messages.
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(0u, sink_->message_count());

  int second_chunk_size = big_buf_size - first_chunk_size;
  result = Send(&buf[first_chunk_size], second_chunk_size);
  EXPECT_EQ(-1, result);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(0u, sink_->message_count());
}

// Tests that when the IPC channel reports an error, that waiting reads are
// unblocked and return a -1 error code.
TEST_F(NaClIPCAdapterTest, ReadWithChannelError) {
  // Have a background thread that waits a bit and calls the channel error
  // handler. This should wake up any waiting threads and immediately return
  // -1. There is an inherent race condition in that we can't be sure if the
  // other thread is actually waiting when this happens. This is OK, since the
  // behavior (which we also explicitly test later) is to return -1 if the
  // channel has already had an error when you start waiting.
  class MyThread : public base::SimpleThread {
   public:
    explicit MyThread(NaClIPCAdapter* adapter)
        : SimpleThread("NaClIPCAdapterThread"),
          adapter_(adapter) {}
    void Run() override {
      base::PlatformThread::Sleep(base::Seconds(1));
      adapter_->OnChannelError();
    }
   private:
    scoped_refptr<NaClIPCAdapter> adapter_;
  };
  MyThread thread(adapter_.get());

  // IMPORTANT: do not return early from here down (including ASSERT_*) because
  // the thread needs to joined or it will assert.
  thread.Start();

  // Request data. This will normally (modulo races) block until data is
  // received or there is an error, and the thread above will wake us up
  // after 1s.
  const int kBufSize = 64;
  char buf[kBufSize];
  int result = BlockingReceive(buf, kBufSize);
  EXPECT_EQ(-1, result);

  // Test the "previously had an error" case. BlockingReceive should return
  // immediately if there was an error.
  result = BlockingReceive(buf, kBufSize);
  EXPECT_EQ(-1, result);

  thread.Join();
}

// Tests that TranslatePepperFileOpenFlags translates pepper read/write open
// flags into NaCl open flags correctly.
TEST_F(NaClIPCAdapterTest, TranslatePepperFileReadWriteOpenFlags) {
  EXPECT_EQ(NACL_ABI_O_RDONLY,
      TranslatePepperFileReadWriteOpenFlagsForTesting(PP_FILEOPENFLAG_READ));
  EXPECT_EQ(NACL_ABI_O_WRONLY,
      TranslatePepperFileReadWriteOpenFlagsForTesting(PP_FILEOPENFLAG_WRITE));
  EXPECT_EQ(NACL_ABI_O_WRONLY | NACL_ABI_O_APPEND,
      TranslatePepperFileReadWriteOpenFlagsForTesting(
          PP_FILEOPENFLAG_APPEND));
  EXPECT_EQ(NACL_ABI_O_RDWR,
      TranslatePepperFileReadWriteOpenFlagsForTesting(
          PP_FILEOPENFLAG_READ | PP_FILEOPENFLAG_WRITE));
  EXPECT_EQ(NACL_ABI_O_WRONLY | NACL_ABI_O_APPEND,
      TranslatePepperFileReadWriteOpenFlagsForTesting(
          PP_FILEOPENFLAG_APPEND));
  EXPECT_EQ(NACL_ABI_O_RDWR | NACL_ABI_O_APPEND,
      TranslatePepperFileReadWriteOpenFlagsForTesting(
          PP_FILEOPENFLAG_READ | PP_FILEOPENFLAG_APPEND));

  // Flags other than PP_FILEOPENFLAG_READ, PP_FILEOPENFLAG_WRITE, and
  // PP_FILEOPENFLAG_APPEND are discarded.
  EXPECT_EQ(NACL_ABI_O_WRONLY,
      TranslatePepperFileReadWriteOpenFlagsForTesting(
          PP_FILEOPENFLAG_WRITE | PP_FILEOPENFLAG_CREATE));
  EXPECT_EQ(NACL_ABI_O_WRONLY,
      TranslatePepperFileReadWriteOpenFlagsForTesting(
          PP_FILEOPENFLAG_WRITE | PP_FILEOPENFLAG_TRUNCATE));
  EXPECT_EQ(NACL_ABI_O_WRONLY,
      TranslatePepperFileReadWriteOpenFlagsForTesting(
          PP_FILEOPENFLAG_WRITE | PP_FILEOPENFLAG_EXCLUSIVE));

  // If none of PP_FILEOPENFLAG_READ, PP_FILEOPENFLAG_WRITE, and
  // PP_FILEOPENFLAG_APPEND are set, the result should fall back to
  // NACL_ABI_O_READONLY.
  EXPECT_EQ(NACL_ABI_O_RDONLY,
      TranslatePepperFileReadWriteOpenFlagsForTesting(0));
  EXPECT_EQ(NACL_ABI_O_RDONLY,
      TranslatePepperFileReadWriteOpenFlagsForTesting(
          PP_FILEOPENFLAG_CREATE));
  EXPECT_EQ(NACL_ABI_O_RDONLY,
      TranslatePepperFileReadWriteOpenFlagsForTesting(
          PP_FILEOPENFLAG_TRUNCATE));
  EXPECT_EQ(NACL_ABI_O_RDONLY,
      TranslatePepperFileReadWriteOpenFlagsForTesting(
          PP_FILEOPENFLAG_EXCLUSIVE));
}
