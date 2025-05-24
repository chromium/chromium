// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/gcm_stats_recorder_impl.h"

#include <stdint.h>

#include <string>

#include "base/containers/circular_deque.h"
#include "components/gcm_driver/crypto/gcm_decryption_result.h"
#include "components/gcm_driver/crypto/gcm_encryption_provider.h"
#include "google_apis/gcm/engine/mcs_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {

namespace {

static uint64_t kAndroidId = 4U;
static const char kCheckinStatus[] = "URL_FETCHING_FAILED";
static const char kHost[] = "www.example.com";
static const char kAppId[] = "app id 1";
static const char kFrom[] = "from";
static const char kSenderIds[] = "s1,s2";
static const char kReceiverId[] = "receiver 1";
static const char kMessageId[] = "message id 1";
static const int kQueuedSec = 5;
static const gcm::MCSClient::MessageSendStatus kMessageSendStatus =
    gcm::MCSClient::QUEUED;
static const int kByteSize = 99;
static const int kTTL = 7;
static const int kRetries = 3;
static const int64_t kDelay = 15000;
static const ConnectionFactory::ConnectionResetReason kReason =
    ConnectionFactory::NETWORK_CHANGE;
static const int kNetworkError = 1;

static const RegistrationRequest::Status kRegistrationStatus =
    RegistrationRequest::SUCCESS;
static const UnregistrationRequest::Status kUnregistrationStatus =
    UnregistrationRequest::SUCCESS;

static const char kCheckinInitiatedEvent[] = "Checkin initiated";
static const char kCheckinInitiatedDetails[] = "Android Id: 4";
static const char kCheckinDelayedDueToBackoffEvent[] = "Checkin backoff";
static const char kCheckinDelayedDueToBackoffDetails[] =
    "Delayed for 15000 msec";
static const char kCheckinSuccessEvent[] = "Checkin succeeded";
static const char kCheckinSuccessDetails[] = "";
static const char kCheckinFailureEvent[] = "Checkin failed";
static const char kCheckinFailureDetails[] = "URL_FETCHING_FAILED. Will retry.";

static const char kConnectionInitiatedEvent[] = "Connection initiated";
static const char kConnectionInitiatedDetails[] = "www.example.com";
static const char kConnectionDelayedDueToBackoffEvent[] = "Connection backoff";
static const char kConnectionDelayedDueToBackoffDetails[] =
    "Delayed for 15000 msec";
static const char kConnectionSuccessEvent[] = "Connection succeeded";
static const char kConnectionSuccessDetails[] = "";
static const char kConnectionFailureEvent[] = "Connection failed";
static const char kConnectionFailureDetails[] = "With network error 1";
static const char kConnectionResetSignaledEvent[] = "Connection reset";
static const char kConnectionResetSignaledDetails[] = "NETWORK_CHANGE";

static const char kRegistrationSentEvent[] = "Registration request sent";
static const char kRegistrationSentDetails[] = "";
static const char kRegistrationResponseEvent[] =
    "Registration response received";
static const char kRegistrationResponseDetails[] = "SUCCESS";
static const char kRegistrationRetryDelayedEvent[] =
    "Registration retry delayed";
static const char kRegistrationRetryDelayedDetails[] =
    "Delayed for 15000 msec, retries left: 3";
static const char kUnregistrationSentEvent[] = "Unregistration request sent";
static const char kUnregistrationSentDetails[] = "";
static const char kUnregistrationResponseEvent[] =
    "Unregistration response received";
static const char kUnregistrationResponseDetails[] = "SUCCESS";
static const char kUnregistrationRetryDelayedEvent[] =
    "Unregistration retry delayed";
static const char kUnregistrationRetryDelayedDetails[] =
    "Delayed for 15000 msec, retries left: 3";

static const char kDataReceivedEvent[] = "Data msg received";
static const char kDataReceivedDetails[] = "";
static const char kDataDeletedMessageEvent[] = "Data msg received";
static const char kDataDeletedMessageDetails[] =
    "Message has been deleted on server";

static const char kDataSentToWireEvent[] = "Data msg sent to wire";
static const char kSentToWireDetails[] = "Msg queued for 5 seconds";
static const char kNotifySendStatusEvent[] = "SEND status: QUEUED";
static const char kNotifySendStatusDetails[] = "Msg size: 99 bytes, TTL: 7";
static const char kIncomingSendErrorEvent[] = "Received 'send error' msg";
static const char kIncomingSendErrorDetails[] = "";

static const GCMDecryptionResult kDecryptionResultFailure =
    GCMDecryptionResult::INVALID_PAYLOAD;

}  // namespace

class GCMStatsRecorderImplTest : public testing::Test {
 public:
  GCMStatsRecorderImplTest();
  ~GCMStatsRecorderImplTest() override;
  void SetUp() override;

  void VerifyRecordedCheckinCount(int expected_count) {
    EXPECT_EQ(expected_count,
              static_cast<int>(recorder_.checkin_activities().size()));
  }
  void VerifyRecordedConnectionCount(int expected_count) {
    EXPECT_EQ(expected_count,
              static_cast<int>(recorder_.connection_activities().size()));
  }
  void VerifyRecordedRegistrationCount(int expected_count) {
    EXPECT_EQ(expected_count,
              static_cast<int>(recorder_.registration_activities().size()));
  }
  void VerifyRecordedReceivingCount(int expected_count) {
    EXPECT_EQ(expected_count,
              static_cast<int>(recorder_.receiving_activities().size()));
  }
  void VerifyRecordedSendingCount(int expected_count) {
    EXPECT_EQ(expected_count,
              static_cast<int>(recorder_.sending_activities().size()));
  }
  void VerifyRecordedDecryptionFailureCount(int expected_count) {
    EXPECT_EQ(
        expected_count,
        static_cast<int>(recorder_.decryption_failure_activities().size()));
  }
  void VerifyAllActivityQueueEmpty(const std::string& remark) {
    EXPECT_TRUE(recorder_.checkin_activities().empty()) << remark;
    EXPECT_TRUE(recorder_.connection_activities().empty()) << remark;
    EXPECT_TRUE(recorder_.registration_activities().empty()) << remark;
    EXPECT_TRUE(recorder_.receiving_activities().empty()) << remark;
    EXPECT_TRUE(recorder_.sending_activities().empty()) << remark;
    EXPECT_TRUE(recorder_.decryption_failure_activities().empty()) << remark;
  }

  void VerifyCheckinInitiated(const std::string& remark) {
    VerifyCheckin(recorder_.checkin_activities(),
                  kCheckinInitiatedEvent,
                  kCheckinInitiatedDetails,
                  remark);
  }

  void VerifyCheckinDelayedDueToBackoff(const std::string& remark) {
    VerifyCheckin(recorder_.checkin_activities(),
                  kCheckinDelayedDueToBackoffEvent,
                  kCheckinDelayedDueToBackoffDetails,
                  remark);
  }

  void VerifyCheckinSuccess(const std::string& remark) {
    VerifyCheckin(recorder_.checkin_activities(),
                  kCheckinSuccessEvent,
                  kCheckinSuccessDetails,
                  remark);
  }

  void VerifyCheckinFailure(const std::string& remark) {
    VerifyCheckin(recorder_.checkin_activities(),
                  kCheckinFailureEvent,
                  kCheckinFailureDetails,
                  remark);
  }

  void VerifyConnectionInitiated(const std::string& remark) {
    VerifyConnection(recorder_.connection_activities(),
                     kConnectionInitiatedEvent,
                     kConnectionInitiatedDetails,
                     remark);
  }

  void VerifyConnectionDelayedDueToBackoff(const std::string& remark) {
    VerifyConnection(recorder_.connection_activities(),
                     kConnectionDelayedDueToBackoffEvent,
                     kConnectionDelayedDueToBackoffDetails,
                     remark);
  }

  void VerifyConnectionSuccess(const std::string& remark) {
    VerifyConnection(recorder_.connection_activities(),
                     kConnectionSuccessEvent,
                     kConnectionSuccessDetails,
                     remark);
  }

  void VerifyConnectionFailure(const std::string& remark) {
    VerifyConnection(recorder_.connection_activities(),
                     kConnectionFailureEvent,
                     kConnectionFailureDetails,
                     remark);
  }

  void VerifyConnectionResetSignaled(const std::string& remark) {
    VerifyConnection(recorder_.connection_activities(),
                     kConnectionResetSignaledEvent,
                     kConnectionResetSignaledDetails,
                     remark);
  }

  void VerifyRegistrationSent(const std::string& remark) {
    VerifyRegistration(recorder_.registration_activities(),
                       kSenderIds,
                       kRegistrationSentEvent,
                       kRegistrationSentDetails,
                       remark);
  }

  void VerifyRegistrationResponse(const std::string& remark) {
    VerifyRegistration(recorder_.registration_activities(),
                       kSenderIds,
                       kRegistrationResponseEvent,
                       kRegistrationResponseDetails,
                       remark);
  }

  void VerifyRegistrationRetryRequested(const std::string& remark) {
    VerifyRegistration(recorder_.registration_activities(),
                       kSenderIds,
                       kRegistrationRetryDelayedEvent,
                       kRegistrationRetryDelayedDetails,
                       remark);
  }

  void VerifyUnregistrationSent(const std::string& remark) {
    VerifyRegistration(recorder_.registration_activities(),
                       kSenderIds,
                       kUnregistrationSentEvent,
                       kUnregistrationSentDetails,
                       remark);
  }

  void VerifyUnregistrationResponse(const std::string& remark) {
    VerifyRegistration(recorder_.registration_activities(),
                       kSenderIds,
                       kUnregistrationResponseEvent,
                       kUnregistrationResponseDetails,
                       remark);
  }

  void VerifyUnregistrationRetryDelayed(const std::string& remark) {
    VerifyRegistration(recorder_.registration_activities(),
                       kSenderIds,
                       kUnregistrationRetryDelayedEvent,
                       kUnregistrationRetryDelayedDetails,
                       remark);
  }

  void VerifyDataMessageReceived(const std::string& remark) {
    VerifyReceivingData(recorder_.receiving_activities(),
                        kDataReceivedEvent,
                        kDataReceivedDetails,
                        remark);
  }

  void VerifyDataDeletedMessage(const std::string& remark) {
    VerifyReceivingData(recorder_.receiving_activities(),
                        kDataDeletedMessageEvent,
                        kDataDeletedMessageDetails,
                        remark);
  }

  void VerifyDataSentToWire(const std::string& remark) {
    VerifySendingData(recorder_.sending_activities(),
                      kDataSentToWireEvent,
                      kSentToWireDetails,
                      remark);
  }

  void VerifyNotifySendStatus(const std::string& remark) {
    VerifySendingData(recorder_.sending_activities(),
                      kNotifySendStatusEvent,
                      kNotifySendStatusDetails,
                      remark);
  }

  void VerifyIncomingSendError(const std::string& remark) {
    VerifySendingData(recorder_.sending_activities(),
                      kIncomingSendErrorEvent,
                      kIncomingSendErrorDetails,
                      remark);
  }

  void VerifyRecordedDecryptionFailure(const std::string& remark) {
    const auto& queue = recorder_.decryption_failure_activities();

    EXPECT_EQ(kAppId, queue.front().app_id) << remark;
    EXPECT_EQ(ToGCMDecryptionResultDetailsString(kDecryptionResultFailure),
              queue.front().details)
        << remark;
  }

 protected:
  void VerifyCheckin(const base::circular_deque<CheckinActivity>& queue,
                     const std::string& event,
                     const std::string& details,
                     const std::string& remark) {
    EXPECT_EQ(event, queue.front().event) << remark;
    EXPECT_EQ(details, queue.front().details) << remark;
  }

  void VerifyConnection(const base::circular_deque<ConnectionActivity>& queue,
                        const std::string& event,
                        const std::string& details,
                        const std::string& remark) {
    EXPECT_EQ(event, queue.front().event) << remark;
    EXPECT_EQ(details, queue.front().details) << remark;
  }

  void VerifyRegistration(
      const base::circular_deque<RegistrationActivity>& queue,
      const std::string& source,
      const std::string& event,
      const std::string& details,
      const std::string& remark) {
    EXPECT_EQ(kAppId, queue.front().app_id) << remark;
    EXPECT_EQ(source, queue.front().source) << remark;
    EXPECT_EQ(event, queue.front().event) << remark;
    EXPECT_EQ(details, queue.front().details) << remark;
  }

  void VerifyReceivingData(const base::circular_deque<ReceivingActivity>& queue,
                           const std::string& event,
                           const std::string& details,
                           const std::string& remark) {
    EXPECT_EQ(kAppId, queue.front().app_id) << remark;
    EXPECT_EQ(kFrom, queue.front().from) << remark;
    EXPECT_EQ(kByteSize, queue.front().message_byte_size) << remark;
    EXPECT_EQ(event, queue.front().event) << remark;
    EXPECT_EQ(details, queue.front().details) << remark;
  }

  void VerifySendingData(const base::circular_deque<SendingActivity>& queue,
                         const std::string& event,
                         const std::string& details,
                         const std::string& remark) {
    EXPECT_EQ(kAppId, queue.front().app_id) << remark;
    EXPECT_EQ(kReceiverId, queue.front().receiver_id) << remark;
    EXPECT_EQ(kMessageId, queue.front().message_id) << remark;
    EXPECT_EQ(event, queue.front().event) << remark;
    EXPECT_EQ(details, queue.front().details) << remark;
  }

  std::string source_;
  GCMStatsRecorderImpl recorder_;
};

GCMStatsRecorderImplTest::GCMStatsRecorderImplTest(){
}

GCMStatsRecorderImplTest::~GCMStatsRecorderImplTest() = default;

void GCMStatsRecorderImplTest::SetUp(){
  source_ = "s1,s2";
  recorder_.set_is_recording(true);
}

TEST_F(GCMStatsRecorderImplTest, StartStopRecordingTest) {
  EXPECT_TRUE(recorder_.is_recording());
  recorder_.RecordDataSentToWire(kAppId, kReceiverId, kMessageId, kQueuedSec);
  VerifyRecordedSendingCount(1);
  VerifyDataSentToWire("1st call");

  recorder_.set_is_recording(false);
  EXPECT_FALSE(recorder_.is_recording());
  recorder_.Clear();
  VerifyAllActivityQueueEmpty("all cleared");

  // Exercise every recording method below and verify that nothing is recorded.
  recorder_.RecordCheckinInitiated(kAndroidId);
  recorder_.RecordCheckinDelayedDueToBackoff(kDelay);
  recorder_.RecordCheckinSuccess();
  recorder_.RecordCheckinFailure(kCheckinStatus, true);
  VerifyAllActivityQueueEmpty("no checkin");

  recorder_.RecordConnectionInitiated(kHost);
  recorder_.RecordConnectionDelayedDueToBackoff(kDelay);
  recorder_.RecordConnectionSuccess();
  recorder_.RecordConnectionFailure(kNetworkError);
  recorder_.RecordConnectionResetSignaled(kReason);
  VerifyAllActivityQueueEmpty("no registration");

  recorder_.RecordRegistrationSent(kAppId, kSenderIds);
  recorder_.RecordRegistrationResponse(kAppId, source_,
                                       kRegistrationStatus);
  recorder_.RecordRegistrationRetryDelayed(kAppId, source_, kDelay, kRetries);
  recorder_.RecordUnregistrationSent(kAppId, source_);
  recorder_.RecordUnregistrationResponse(
      kAppId, source_, kUnregistrationStatus);
  recorder_.RecordUnregistrationRetryDelayed(kAppId, source_, kDelay, kRetries);
  VerifyAllActivityQueueEmpty("no unregistration");

  recorder_.RecordDataMessageReceived(kAppId, kFrom, kByteSize,
                                      GCMStatsRecorder::DATA_MESSAGE);
  recorder_.RecordDataMessageReceived(kAppId, kFrom, kByteSize,
                                      GCMStatsRecorder::DELETED_MESSAGES);
  VerifyAllActivityQueueEmpty("no receiving");

  recorder_.RecordDataSentToWire(kAppId, kReceiverId, kMessageId, kQueuedSec);
  recorder_.RecordNotifySendStatus(kAppId, kReceiverId, kMessageId,
                                   kMessageSendStatus, kByteSize, kTTL);
  recorder_.RecordIncomingSendError(kAppId, kReceiverId, kMessageId);
  recorder_.RecordDataSentToWire(kAppId, kReceiverId, kMessageId, kQueuedSec);
  VerifyAllActivityQueueEmpty("no sending");
}

TEST_F(GCMStatsRecorderImplTest, ClearLogTest) {
  recorder_.RecordDataSentToWire(kAppId, kReceiverId, kMessageId, kQueuedSec);
  VerifyRecordedSendingCount(1);
  VerifyDataSentToWire("1st call");

  recorder_.RecordNotifySendStatus(kAppId, kReceiverId, kMessageId,
                                   kMessageSendStatus, kByteSize, kTTL);
  VerifyRecordedSendingCount(2);
  VerifyNotifySendStatus("2nd call");

  recorder_.Clear();
  VerifyRecordedSendingCount(0);
}

TEST_F(GCMStatsRecorderImplTest, CheckinTest) {
  recorder_.RecordCheckinInitiated(kAndroidId);
  VerifyRecordedCheckinCount(1);
  VerifyCheckinInitiated("1st call");

  recorder_.RecordCheckinDelayedDueToBackoff(kDelay);
  VerifyRecordedCheckinCount(2);
  VerifyCheckinDelayedDueToBackoff("2nd call");

  recorder_.RecordCheckinSuccess();
  VerifyRecordedCheckinCount(3);
  VerifyCheckinSuccess("3rd call");

  recorder_.RecordCheckinFailure(kCheckinStatus, true);
  VerifyRecordedCheckinCount(4);
  VerifyCheckinFailure("4th call");
}

TEST_F(GCMStatsRecorderImplTest, ConnectionTest) {
  recorder_.RecordConnectionInitiated(kHost);
  VerifyRecordedConnectionCount(1);
  VerifyConnectionInitiated("1st call");

  recorder_.RecordConnectionDelayedDueToBackoff(kDelay);
  VerifyRecordedConnectionCount(2);
  VerifyConnectionDelayedDueToBackoff("2nd call");

  recorder_.RecordConnectionSuccess();
  VerifyRecordedConnectionCount(3);
  VerifyConnectionSuccess("3rd call");

  recorder_.RecordConnectionFailure(kNetworkError);
  VerifyRecordedConnectionCount(4);
  VerifyConnectionFailure("4th call");

  recorder_.RecordConnectionResetSignaled(kReason);
  VerifyRecordedConnectionCount(5);
  VerifyConnectionResetSignaled("5th call");
}

TEST_F(GCMStatsRecorderImplTest, RegistrationTest) {
  recorder_.RecordRegistrationSent(kAppId, kSenderIds);
  VerifyRecordedRegistrationCount(1);
  VerifyRegistrationSent("1st call");

  recorder_.RecordRegistrationResponse(kAppId, source_,
                                       kRegistrationStatus);
  VerifyRecordedRegistrationCount(2);
  VerifyRegistrationResponse("2nd call");

  recorder_.RecordRegistrationRetryDelayed(kAppId, source_, kDelay, kRetries);
  VerifyRecordedRegistrationCount(3);
  VerifyRegistrationRetryRequested("3rd call");

  recorder_.RecordUnregistrationSent(kAppId, source_);
  VerifyRecordedRegistrationCount(4);
  VerifyUnregistrationSent("4th call");

  recorder_.RecordUnregistrationResponse(
      kAppId, source_, kUnregistrationStatus);
  VerifyRecordedRegistrationCount(5);
  VerifyUnregistrationResponse("5th call");

  recorder_.RecordUnregistrationRetryDelayed(kAppId, source_, kDelay, kRetries);
  VerifyRecordedRegistrationCount(6);
  VerifyUnregistrationRetryDelayed("6th call");
}

TEST_F(GCMStatsRecorderImplTest, RecordReceivingTest) {
  recorder_.RecordConnectionInitiated(std::string());
  recorder_.RecordConnectionSuccess();
  recorder_.RecordDataMessageReceived(kAppId, kFrom, kByteSize,
                                      GCMStatsRecorder::DATA_MESSAGE);
  VerifyRecordedReceivingCount(1);
  VerifyDataMessageReceived("1st call");

  recorder_.RecordDataMessageReceived(kAppId, kFrom, kByteSize,
                                      GCMStatsRecorder::DELETED_MESSAGES);
  VerifyRecordedReceivingCount(2);
  VerifyDataDeletedMessage("2nd call");
}

TEST_F(GCMStatsRecorderImplTest, RecordSendingTest) {
  recorder_.RecordDataSentToWire(kAppId, kReceiverId, kMessageId, kQueuedSec);
  VerifyRecordedSendingCount(1);
  VerifyDataSentToWire("1st call");

  recorder_.RecordNotifySendStatus(kAppId, kReceiverId, kMessageId,
                                   kMessageSendStatus, kByteSize, kTTL);
  VerifyRecordedSendingCount(2);
  VerifyNotifySendStatus("2nd call");

  recorder_.RecordIncomingSendError(kAppId, kReceiverId, kMessageId);
  VerifyRecordedSendingCount(3);
  VerifyIncomingSendError("3rd call");

  recorder_.RecordDataSentToWire(kAppId, kReceiverId, kMessageId, kQueuedSec);
  VerifyRecordedSendingCount(4);
  VerifyDataSentToWire("4th call");
}

TEST_F(GCMStatsRecorderImplTest, RecordDecryptionFailureTest) {
  recorder_.RecordDecryptionFailure(kAppId, kDecryptionResultFailure);
  VerifyRecordedDecryptionFailureCount(1);

  VerifyRecordedDecryptionFailure("1st call");
}

}  // namespace gcm
