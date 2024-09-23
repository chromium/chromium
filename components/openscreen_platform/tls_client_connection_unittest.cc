// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/openscreen_platform/tls_client_connection.h"

#include <cstring>
#include <iterator>
#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Mock;
using ::testing::StrictMock;

namespace openscreen_platform {

using openscreen::Error;
using openscreen::TlsConnection;

namespace {
const openscreen::IPEndpoint kValidEndpointOne{
    openscreen::IPAddress{192, 168, 0, 1}, 80};
const openscreen::IPEndpoint kValidEndpointTwo{
    openscreen::IPAddress{10, 9, 8, 7}, 81};

constexpr int kDataPipeCapacity = 32;

const uint8_t kTestMessage[] = "Hello world!";

// Creates two data pipes, one for inbound data and one for outbound data, and
// provides test utilities for simulating socket stream events of interest.
class FakeSocketStreams {
 public:
  FakeSocketStreams()
      : outbound_stream_watcher_(FROM_HERE,
                                 mojo::SimpleWatcher::ArmingPolicy::MANUAL) {
    MojoCreateDataPipeOptions options{};
    options.struct_size = sizeof(options);
    options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
    options.element_num_bytes = 1;
    options.capacity_num_bytes = kDataPipeCapacity;
    MojoResult result =
        CreateDataPipe(&options, inbound_stream_, receive_stream_);
    CHECK_EQ(result, MOJO_RESULT_OK);
    result = CreateDataPipe(&options, send_stream_, outbound_stream_);
    CHECK_EQ(result, MOJO_RESULT_OK);

    outbound_stream_watcher_.Watch(
        outbound_stream_.get(),
        MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED |
            MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE,
        MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
        base::BindRepeating(&FakeSocketStreams::OnOutboundStreamActivity,
                            base::Unretained(this)));
    outbound_stream_watcher_.ArmOrNotify();
  }

  ~FakeSocketStreams() = default;

  // These should be passed to the TlsClientConnection constructor.
  mojo::ScopedDataPipeConsumerHandle TakeReceiveStream() {
    return std::move(receive_stream_);
  }
  mojo::ScopedDataPipeProducerHandle TakeSendStream() {
    return std::move(send_stream_);
  }

  // Writes data into the inbound data pipe, which should ultimately result in a
  // TlsClientConnection::Client's OnRead() method being called.
  void SimulateSocketReceive(base::span<const uint8_t> data) {
    const MojoResult result = inbound_stream_->WriteAllData(data);
    ASSERT_EQ(result, MOJO_RESULT_OK);
  }

  // Closes the inbound (or outbound) data pipe to allow the unit tests to check
  // the error handling of TlsClientConnection.
  void SimulateInboundClose() { inbound_stream_.reset(); }
  void SimulateOutboundClose() { outbound_stream_.reset(); }

  // Returns all outbound stream data accumulated so far, and clears the
  // internal buffer.
  std::vector<uint8_t> TakeAccumulatedOutboundData() {
    std::vector<uint8_t> result;
    result.swap(outbound_data_);
    return result;
  }

 private:
  // Mojo SimpleWatcher callback to save all data being sent from a connection.
  void OnOutboundStreamActivity(MojoResult result,
                                const mojo::HandleSignalsState& state) {
    if (!outbound_stream_.is_valid()) {
      return;
    }
    ASSERT_EQ(result, MOJO_RESULT_OK);

    size_t num_bytes = 0;
    result = outbound_stream_->ReadData(MOJO_READ_DATA_FLAG_QUERY,
                                        base::span<uint8_t>(), num_bytes);
    ASSERT_EQ(result, MOJO_RESULT_OK);
    size_t old_end_index = outbound_data_.size();
    outbound_data_.resize(old_end_index + num_bytes);
    result = outbound_stream_->ReadData(
        MOJO_READ_DATA_FLAG_NONE,
        base::span(outbound_data_).subspan(old_end_index), num_bytes);
    ASSERT_EQ(result, MOJO_RESULT_OK);
    outbound_data_.resize(old_end_index + num_bytes);

    outbound_stream_watcher_.ArmOrNotify();
  }

  mojo::ScopedDataPipeProducerHandle inbound_stream_;
  mojo::ScopedDataPipeConsumerHandle receive_stream_;

  mojo::ScopedDataPipeProducerHandle send_stream_;
  mojo::ScopedDataPipeConsumerHandle outbound_stream_;

  mojo::SimpleWatcher outbound_stream_watcher_;
  std::vector<uint8_t> outbound_data_;
};

class MockTlsConnectionClient : public TlsConnection::Client {
 public:
  MOCK_METHOD(void, OnError, (TlsConnection*, const Error&), (override));
  MOCK_METHOD(void, OnRead, (TlsConnection*, std::vector<uint8_t>), (override));
};

}  // namespace

class TlsClientConnectionTest : public ::testing::Test {
 public:
  TlsClientConnectionTest() = default;
  ~TlsClientConnectionTest() override = default;

  void SetUp() override {
    client_ = std::make_unique<StrictMock<MockTlsConnectionClient>>();
    socket_streams_ = std::make_unique<FakeSocketStreams>();
    connection_ = std::make_unique<TlsClientConnection>(
        kValidEndpointOne, kValidEndpointTwo,
        socket_streams_->TakeReceiveStream(), socket_streams_->TakeSendStream(),
        mojo::Remote<network::mojom::TCPConnectedSocket>{},
        mojo::Remote<network::mojom::TLSClientSocket>{});
  }

  void TearDown() override {
    connection_.reset();
    socket_streams_.reset();
    client_.reset();
    base::RunLoop().RunUntilIdle();
  }

  StrictMock<MockTlsConnectionClient>* client() const { return client_.get(); }
  FakeSocketStreams* socket_streams() const { return socket_streams_.get(); }
  TlsClientConnection* connection() const { return connection_.get(); }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<StrictMock<MockTlsConnectionClient>> client_;
  std::unique_ptr<FakeSocketStreams> socket_streams_;
  std::unique_ptr<TlsClientConnection> connection_;
};

TEST_F(TlsClientConnectionTest, CallsClientOnReadForInboundData) {
  // Test multiple reads to confirm the data pipe watcher is being re-armed
  // correctly after each read.
  constexpr int kNumReads = 3;

  connection()->SetClient(client());

  for (int i = 0; i < kNumReads; ++i) {
    // Send a different message in each iteration.
    std::vector<uint8_t> expected_data(std::begin(kTestMessage),
                                       std::end(kTestMessage));
    for (uint8_t& byte : expected_data) {
      byte ^= i;
    }
    EXPECT_CALL(*client(), OnRead(connection(), expected_data)).Times(1);
    socket_streams()->SimulateSocketReceive(expected_data);
    base::RunLoop().RunUntilIdle();
    Mock::VerifyAndClearExpectations(client());
  }
}

TEST_F(TlsClientConnectionTest, CallsClientOnErrorWhenSocketInboundCloses) {
  EXPECT_CALL(*client(), OnError(connection(), _)).Times(1);
  connection()->SetClient(client());

  socket_streams()->SimulateInboundClose();
  base::RunLoop().RunUntilIdle();
}

TEST_F(TlsClientConnectionTest, SendsUntilBlocked) {
  // Note: Client::OnError() should not be called during this test since an
  // outbound-blocked socket is not a fatal error.
  EXPECT_CALL(*client(), OnError(connection(), _)).Times(0);
  connection()->SetClient(client());

  std::vector<uint8_t> message(kDataPipeCapacity / 2);
  for (int i = 0; i < kDataPipeCapacity / 2; ++i) {
    message[i] = static_cast<uint8_t>(i);
  }

  // Send one message whose size is half the pipe's capacity.
  EXPECT_TRUE(connection()->Send(message));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(message, socket_streams()->TakeAccumulatedOutboundData());

  // Send two messages whose sizes are half the pipe's capacity.
  EXPECT_TRUE(connection()->Send(message));
  EXPECT_TRUE(connection()->Send(message));
  base::RunLoop().RunUntilIdle();
  std::vector<uint8_t> accumulated_data =
      socket_streams()->TakeAccumulatedOutboundData();
  ASSERT_EQ(message.size() * 2, accumulated_data.size());
  EXPECT_EQ(0, memcmp(message.data(), accumulated_data.data(), message.size()));
  EXPECT_EQ(0, memcmp(message.data(), accumulated_data.data() + message.size(),
                      message.size()));

  // Attempt to send three messages, but expect the third to fail.
  EXPECT_TRUE(connection()->Send(message));
  EXPECT_TRUE(connection()->Send(message));
  EXPECT_FALSE(connection()->Send(message));
  base::RunLoop().RunUntilIdle();
  accumulated_data = socket_streams()->TakeAccumulatedOutboundData();
  ASSERT_EQ(message.size() * 2, accumulated_data.size());
  EXPECT_EQ(0, memcmp(message.data(), accumulated_data.data(), message.size()));
  EXPECT_EQ(0, memcmp(message.data(), accumulated_data.data() + message.size(),
                      message.size()));

  // Sending should resume when there is capacity available again.
  EXPECT_TRUE(connection()->Send(message));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(message, socket_streams()->TakeAccumulatedOutboundData());
}

TEST_F(TlsClientConnectionTest,
       CallsClientOnErrorWhenSendingToClosedOutboundStream) {
  EXPECT_CALL(*client(), OnError(connection(), _)).Times(0);
  connection()->SetClient(client());

  // Send a message and immediately close the outbound stream.
  EXPECT_TRUE(connection()->Send(kTestMessage));
  socket_streams()->SimulateOutboundClose();
  base::RunLoop().RunUntilIdle();

  // The Client should not have encountered any fatal errors yet.
  Mock::VerifyAndClearExpectations(client());

  // Now, call Send() again and this should trigger a fatal error.
  EXPECT_CALL(*client(), OnError(connection(), _)).Times(1);
  EXPECT_FALSE(connection()->Send(kTestMessage));
}

TEST_F(TlsClientConnectionTest, CanRetrieveAddresses) {
  EXPECT_EQ(kValidEndpointTwo, connection()->GetRemoteEndpoint());
}

}  // namespace openscreen_platform
