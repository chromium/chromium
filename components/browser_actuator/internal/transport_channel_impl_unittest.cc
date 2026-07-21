// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/transport_channel_impl.h"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "components/browser_actuator/internal/proto/transport_messages.pb.h"
#include "components/browser_actuator/internal/transport/message_stream_client.h"
#include "components/browser_actuator/internal/transport/stream_connection_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_actuator {
namespace {

using ::testing::AllOf;
using ::testing::Property;
using ::testing::UnorderedElementsAre;

// Minimal MessageStreamClient that lets the test push downstream messages
// straight to the channel's observer.
class FakeMessageStreamClient : public MessageStreamClient {
 public:
  void Connect() override {}
  void Disconnect() override {}
  bool IsConnected() const override { return false; }
  void AddObserver(Observer* observer) override { observer_ = observer; }
  void RemoveObserver(Observer* observer) override {
    if (observer_ == observer) {
      observer_ = nullptr;
    }
  }

  void Dispatch(const std::string& message) {
    ASSERT_TRUE(observer_);
    observer_->OnStreamMessage(message);
  }

 private:
  raw_ptr<Observer> observer_ = nullptr;
};

std::string SerializedDownstream(std::string_view session_id, int64_t seq) {
  ActuatorDownstreamMessage message;
  message.set_session_id(session_id);
  message.set_sequence_number(seq);
  return message.SerializeAsString();
}

class TransportChannelImplTest : public testing::Test {
 protected:
  TransportChannelImplTest() {
    auto client = std::make_unique<FakeMessageStreamClient>();
    fake_client_ = client.get();
    channel_ = std::make_unique<TransportChannelImpl>(base::BindOnce(
        [](std::unique_ptr<FakeMessageStreamClient> owned_client,
           std::unique_ptr<StreamConnectionDelegate>)
            -> std::unique_ptr<MessageStreamClient> {
          // The resume delegate isn't exercised here; the test drives the
          // body builder directly.
          return std::move(owned_client);
        },
        std::move(client)));
  }

  WatchSessionsRequest ResumeBody() {
    WatchSessionsRequest request;
    EXPECT_TRUE(request.ParseFromString(
        channel_->BuildWatchSessionsRequestBodyForTesting()));
    return request;
  }

  std::unique_ptr<TransportChannelImpl> channel_;
  raw_ptr<FakeMessageStreamClient> fake_client_;
};

TEST_F(TransportChannelImplTest, RecordsPerSessionResumePositions) {
  fake_client_->Dispatch(SerializedDownstream("s1", 5));
  fake_client_->Dispatch(SerializedDownstream("s2", 2));

  const WatchSessionsRequest body = ResumeBody();
  EXPECT_FALSE(body.request_id().empty());
  EXPECT_THAT(
      body.sessions(),
      UnorderedElementsAre(
          AllOf(Property(&WatchSessionsRequest::Session::session_id, "s1"),
                Property(
                    &WatchSessionsRequest::Session::last_seen_sequence_number,
                    5)),
          AllOf(Property(&WatchSessionsRequest::Session::session_id, "s2"),
                Property(
                    &WatchSessionsRequest::Session::last_seen_sequence_number,
                    2))));
}

TEST_F(TransportChannelImplTest, AdvanceIsMonotonicAndIgnoresNonPositive) {
  fake_client_->Dispatch(SerializedDownstream("s1", 5));
  fake_client_->Dispatch(SerializedDownstream("s1", 3));  // Out of order.
  fake_client_->Dispatch(SerializedDownstream("s1", 0));  // Unset.

  const WatchSessionsRequest body = ResumeBody();
  ASSERT_EQ(body.sessions_size(), 1);
  EXPECT_EQ(body.sessions(0).session_id(), "s1");
  EXPECT_EQ(body.sessions(0).last_seen_sequence_number(), 5);
}

TEST_F(TransportChannelImplTest, SessionWithoutResumePositionIsOmitted) {
  // A session that only produced an unset (0) sequence number has no resume
  // position, so it must not appear in the watch request.
  fake_client_->Dispatch(SerializedDownstream("s1", 0));

  EXPECT_EQ(ResumeBody().sessions_size(), 0);
}

}  // namespace
}  // namespace browser_actuator
