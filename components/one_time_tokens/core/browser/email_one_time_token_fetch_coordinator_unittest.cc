// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/core/browser/email_one_time_token_fetch_coordinator.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace one_time_tokens {

namespace {

class MockDelegate : public EmailOneTimeTokenFetchCoordinator::Delegate {
 public:
  MOCK_METHOD(void,
              OnCanSendNetworkRequest,
              (const OneTimeTokenBackendNotification& notification,
               base::TimeTicks trigger_time),
              (override));
};

class EmailOneTimeTokenFetchCoordinatorTest : public testing::Test {
 public:
  EmailOneTimeTokenFetchCoordinatorTest() : coordinator_(mock_delegate_) {}

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  MockDelegate mock_delegate_;
  EmailOneTimeTokenFetchCoordinator coordinator_;
  base::HistogramTester histogram_tester_;
};

MATCHER_P(OneTimeTokenNotificationMatches, expected_reference, "") {
  return arg.encrypted_message_reference.value() == expected_reference;
}

// Tests that the coordinator signals the delegate when a request is needed.
TEST_F(EmailOneTimeTokenFetchCoordinatorTest, SignalNetworkRequestNeeded) {
  EXPECT_CALL(
      mock_delegate_,
      OnCanSendNetworkRequest(OneTimeTokenNotificationMatches("test_reference"),
                              testing::_));

  coordinator_.SignalNetworkRequestNeeded(OneTimeTokenBackendNotification(
      EncryptedMessageReference("test_reference")));

  histogram_tester_.ExpectUniqueSample(
      "Autofill.OneTimeTokens.Backend.Gmail.QueueSize", 1, 1);
}

// Tests that the coordinator enforces the concurrency limit.
TEST_F(EmailOneTimeTokenFetchCoordinatorTest, EnforcesConcurrencyLimit) {
  const OneTimeTokenBackendNotification notification1(
      EncryptedMessageReference("ref1"));
  const OneTimeTokenBackendNotification notification2(
      EncryptedMessageReference("ref2"));
  const OneTimeTokenBackendNotification notification3(
      EncryptedMessageReference("ref3"));
  const OneTimeTokenBackendNotification notification4(
      EncryptedMessageReference("ref4"));
  const OneTimeTokenBackendNotification notification5(
      EncryptedMessageReference("ref5"));

  // Only the first 3 should be allowed immediately as kMaxConcurrentRequests
  // is 3.
  EXPECT_CALL(mock_delegate_,
              OnCanSendNetworkRequest(OneTimeTokenNotificationMatches("ref1"),
                                      testing::_));
  EXPECT_CALL(mock_delegate_,
              OnCanSendNetworkRequest(OneTimeTokenNotificationMatches("ref2"),
                                      testing::_));
  EXPECT_CALL(mock_delegate_,
              OnCanSendNetworkRequest(OneTimeTokenNotificationMatches("ref3"),
                                      testing::_));
  EXPECT_CALL(mock_delegate_,
              OnCanSendNetworkRequest(OneTimeTokenNotificationMatches("ref4"),
                                      testing::_))
      .Times(0);
  EXPECT_CALL(mock_delegate_,
              OnCanSendNetworkRequest(OneTimeTokenNotificationMatches("ref5"),
                                      testing::_))
      .Times(0);

  coordinator_.SignalNetworkRequestNeeded(notification1);
  coordinator_.SignalNetworkRequestNeeded(notification2);
  coordinator_.SignalNetworkRequestNeeded(notification3);
  coordinator_.SignalNetworkRequestNeeded(notification4);
  coordinator_.SignalNetworkRequestNeeded(notification5);

  // QueueSize is recorded after push_back and before ProcessQueue.
  // - Notifications 1-3: Added to an empty queue (size 1) and immediately
  //   moved to active requests.
  // - Notification 4: Added to an empty queue (size 1) but active requests
  //   are full (limit 3), so it remains pending.
  // - Notification 5: Added to a queue containing notification 4 (size 2).
  histogram_tester_.ExpectBucketCount(
      "Autofill.OneTimeTokens.Backend.Gmail.QueueSize", 1, 4);
  histogram_tester_.ExpectBucketCount(
      "Autofill.OneTimeTokens.Backend.Gmail.QueueSize", 2, 1);
}

// Tests that the coordinator ignores duplicate tickles for the same
// notification.
TEST_F(EmailOneTimeTokenFetchCoordinatorTest, DeDuplicatesIncomingTickles) {
  const OneTimeTokenBackendNotification notification(
      EncryptedMessageReference("test_reference"));

  // Only 1 request lifecycle should be started even if multiple tickles arrive.
  EXPECT_CALL(
      mock_delegate_,
      OnCanSendNetworkRequest(OneTimeTokenNotificationMatches("test_reference"),
                              testing::_))
      .Times(1);

  coordinator_.SignalNetworkRequestNeeded(notification);
  coordinator_.SignalNetworkRequestNeeded(notification);
  coordinator_.SignalNetworkRequestNeeded(notification);

  // Histogram should only be recorded once for the non-duplicate request.
  histogram_tester_.ExpectUniqueSample(
      "Autofill.OneTimeTokens.Backend.Gmail.QueueSize", 1, 1);
}

// Tests that the coordinator ignores duplicate tickles for the same reference
// even if the timestamp is different.
TEST_F(EmailOneTimeTokenFetchCoordinatorTest,
       DeDuplicatesIncomingTicklesWithDifferentTimestamps) {
  const EncryptedMessageReference reference("test_reference");
  const OneTimeTokenBackendNotification notification1(
      reference, base::Time::FromTimeT(100), base::Time::FromTimeT(100),
      base::Time::FromTimeT(100), base::Time::FromTimeT(100),
      base::TimeTicks::Now());
  const OneTimeTokenBackendNotification notification2(
      reference, base::Time::FromTimeT(200), base::Time::FromTimeT(200),
      base::Time::FromTimeT(200), base::Time::FromTimeT(200),
      base::TimeTicks::Now());

  // Only 1 request lifecycle should be started.
  EXPECT_CALL(
      mock_delegate_,
      OnCanSendNetworkRequest(OneTimeTokenNotificationMatches("test_reference"),
                              testing::_))
      .Times(1);

  coordinator_.SignalNetworkRequestNeeded(notification1);
  coordinator_.SignalNetworkRequestNeeded(notification2);
}

// Tests that finishing a request allows a pending request to start.
TEST_F(EmailOneTimeTokenFetchCoordinatorTest, ProcessesQueueOnCompletion) {
  const OneTimeTokenBackendNotification notification1(
      EncryptedMessageReference("ref1"));
  const OneTimeTokenBackendNotification notification2(
      EncryptedMessageReference("ref2"));
  const OneTimeTokenBackendNotification notification3(
      EncryptedMessageReference("ref3"));
  const OneTimeTokenBackendNotification notification4(
      EncryptedMessageReference("ref4"));

  EXPECT_CALL(mock_delegate_,
              OnCanSendNetworkRequest(OneTimeTokenNotificationMatches("ref1"),
                                      testing::_));
  EXPECT_CALL(mock_delegate_,
              OnCanSendNetworkRequest(OneTimeTokenNotificationMatches("ref2"),
                                      testing::_));
  EXPECT_CALL(mock_delegate_,
              OnCanSendNetworkRequest(OneTimeTokenNotificationMatches("ref3"),
                                      testing::_));

  coordinator_.SignalNetworkRequestNeeded(notification1);
  coordinator_.SignalNetworkRequestNeeded(notification2);
  coordinator_.SignalNetworkRequestNeeded(notification3);
  coordinator_.SignalNetworkRequestNeeded(notification4);

  // Finishing notification1 should trigger notification4.
  EXPECT_CALL(mock_delegate_,
              OnCanSendNetworkRequest(OneTimeTokenNotificationMatches("ref4"),
                                      testing::_));
  coordinator_.InformOfNetworkRequestFinished(notification1);
}

// Tests de-duplication when a notification is already in the pending queue.
TEST_F(EmailOneTimeTokenFetchCoordinatorTest, DeDuplicatesPendingRequests) {
  const OneTimeTokenBackendNotification notification1(
      EncryptedMessageReference("ref1"));
  const OneTimeTokenBackendNotification notification2(
      EncryptedMessageReference("ref2"));
  const OneTimeTokenBackendNotification notification3(
      EncryptedMessageReference("ref3"));
  const OneTimeTokenBackendNotification notification4(
      EncryptedMessageReference("ref4"));

  EXPECT_CALL(mock_delegate_,
              OnCanSendNetworkRequest(OneTimeTokenNotificationMatches("ref1"),
                                      testing::_));
  EXPECT_CALL(mock_delegate_,
              OnCanSendNetworkRequest(OneTimeTokenNotificationMatches("ref2"),
                                      testing::_));
  EXPECT_CALL(mock_delegate_,
              OnCanSendNetworkRequest(OneTimeTokenNotificationMatches("ref3"),
                                      testing::_));

  coordinator_.SignalNetworkRequestNeeded(notification1);
  coordinator_.SignalNetworkRequestNeeded(notification2);
  coordinator_.SignalNetworkRequestNeeded(notification3);

  // notification4 is now pending.
  coordinator_.SignalNetworkRequestNeeded(notification4);

  // Signal notification4 again. It should still be de-duplicated.
  coordinator_.SignalNetworkRequestNeeded(notification4);

  // Finishing notification1 should trigger notification4 only once.
  EXPECT_CALL(mock_delegate_,
              OnCanSendNetworkRequest(OneTimeTokenNotificationMatches("ref4"),
                                      testing::_))
      .Times(1);
  coordinator_.InformOfNetworkRequestFinished(notification1);
}

// Tests that the coordinator correctly records the queue latency.
TEST_F(EmailOneTimeTokenFetchCoordinatorTest, RecordsQueueLatency) {
  const OneTimeTokenBackendNotification notification1(
      EncryptedMessageReference("ref1"));
  const OneTimeTokenBackendNotification notification2(
      EncryptedMessageReference("ref2"));
  const OneTimeTokenBackendNotification notification3(
      EncryptedMessageReference("ref3"));
  const OneTimeTokenBackendNotification notification4(
      EncryptedMessageReference("ref4"));

  // Max concurrent requests is 3. The first 3 should be dispatched immediately.
  EXPECT_CALL(mock_delegate_,
              OnCanSendNetworkRequest(OneTimeTokenNotificationMatches("ref1"),
                                      testing::_));
  EXPECT_CALL(mock_delegate_,
              OnCanSendNetworkRequest(OneTimeTokenNotificationMatches("ref2"),
                                      testing::_));
  EXPECT_CALL(mock_delegate_,
              OnCanSendNetworkRequest(OneTimeTokenNotificationMatches("ref3"),
                                      testing::_));

  coordinator_.SignalNetworkRequestNeeded(notification1);
  coordinator_.SignalNetworkRequestNeeded(notification2);
  coordinator_.SignalNetworkRequestNeeded(notification3);

  // The 4th notification is queued.
  base::TimeTicks entry_time = base::TimeTicks::Now();
  coordinator_.SignalNetworkRequestNeeded(notification4);

  // Fast-forward time by 500ms to simulate the notification waiting in the
  // queue.
  task_environment_.FastForwardBy(base::Milliseconds(500));

  // Finish the first request to free up a slot for the 4th.
  EXPECT_CALL(mock_delegate_,
              OnCanSendNetworkRequest(OneTimeTokenNotificationMatches("ref4"),
                                      testing::Eq(entry_time)));
  coordinator_.InformOfNetworkRequestFinished(notification1);

  // Validate the histogram is recorded with the exact latency.
  histogram_tester_.ExpectTimeBucketCount(
      "Autofill.OneTimeTokens.Backend.Gmail.QueueLatency",
      base::Milliseconds(500), 1);
  histogram_tester_.ExpectTotalCount(
      "Autofill.OneTimeTokens.Backend.Gmail.QueueLatency", 4);
}

}  // namespace

}  // namespace one_time_tokens
