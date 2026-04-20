// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/core/browser/email_one_time_token_fetch_coordinator.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace one_time_tokens {

namespace {

class MockDelegate : public EmailOneTimeTokenFetchCoordinator::Delegate {
 public:
  MOCK_METHOD(void,
              OnCanSendNetworkRequest,
              (const OneTimeTokenBackendNotification& notification),
              (override));
};

class EmailOneTimeTokenFetchCoordinatorTest : public testing::Test {
 public:
  EmailOneTimeTokenFetchCoordinatorTest() : coordinator_(mock_delegate_) {}

 protected:
  MockDelegate mock_delegate_;
  EmailOneTimeTokenFetchCoordinator coordinator_;
};

MATCHER_P(OneTimeTokenNotificationMatches, expected_reference, "") {
  return arg.encrypted_message_reference.value() == expected_reference;
}

// Tests that the coordinator signals the delegate when a request is needed.
TEST_F(EmailOneTimeTokenFetchCoordinatorTest, SignalNetworkRequestNeeded) {
  EXPECT_CALL(mock_delegate_,
              OnCanSendNetworkRequest(
                  OneTimeTokenNotificationMatches("test_reference")));

  coordinator_.SignalNetworkRequestNeeded(OneTimeTokenBackendNotification(
      EncryptedMessageReference("test_reference")));
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
              OnCanSendNetworkRequest(OneTimeTokenNotificationMatches("ref1")));
  EXPECT_CALL(mock_delegate_,
              OnCanSendNetworkRequest(OneTimeTokenNotificationMatches("ref2")));
  EXPECT_CALL(mock_delegate_,
              OnCanSendNetworkRequest(OneTimeTokenNotificationMatches("ref3")));
  EXPECT_CALL(mock_delegate_,
              OnCanSendNetworkRequest(OneTimeTokenNotificationMatches("ref4")))
      .Times(0);
  EXPECT_CALL(mock_delegate_,
              OnCanSendNetworkRequest(OneTimeTokenNotificationMatches("ref5")))
      .Times(0);

  coordinator_.SignalNetworkRequestNeeded(notification1);
  coordinator_.SignalNetworkRequestNeeded(notification2);
  coordinator_.SignalNetworkRequestNeeded(notification3);
  coordinator_.SignalNetworkRequestNeeded(notification4);
  coordinator_.SignalNetworkRequestNeeded(notification5);
}

// Tests that the coordinator ignores duplicate tickles for the same
// notification.
TEST_F(EmailOneTimeTokenFetchCoordinatorTest, DeDuplicatesIncomingTickles) {
  const OneTimeTokenBackendNotification notification(
      EncryptedMessageReference("test_reference"));

  // Only 1 request lifecycle should be started even if multiple tickles arrive.
  EXPECT_CALL(mock_delegate_,
              OnCanSendNetworkRequest(
                  OneTimeTokenNotificationMatches("test_reference")))
      .Times(1);

  coordinator_.SignalNetworkRequestNeeded(notification);
  coordinator_.SignalNetworkRequestNeeded(notification);
  coordinator_.SignalNetworkRequestNeeded(notification);
}

// Tests that the coordinator ignores duplicate tickles for the same reference
// even if the timestamp is different.
TEST_F(EmailOneTimeTokenFetchCoordinatorTest,
       DeDuplicatesIncomingTicklesWithDifferentTimestamps) {
  const EncryptedMessageReference reference("test_reference");
  const OneTimeTokenBackendNotification notification1(
      reference, base::Time::FromTimeT(100), base::Time::FromTimeT(100),
      base::Time::FromTimeT(100));
  const OneTimeTokenBackendNotification notification2(
      reference, base::Time::FromTimeT(200), base::Time::FromTimeT(200),
      base::Time::FromTimeT(200));

  // Only 1 request lifecycle should be started.
  EXPECT_CALL(mock_delegate_,
              OnCanSendNetworkRequest(
                  OneTimeTokenNotificationMatches("test_reference")))
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
              OnCanSendNetworkRequest(OneTimeTokenNotificationMatches("ref1")));
  EXPECT_CALL(mock_delegate_,
              OnCanSendNetworkRequest(OneTimeTokenNotificationMatches("ref2")));
  EXPECT_CALL(mock_delegate_,
              OnCanSendNetworkRequest(OneTimeTokenNotificationMatches("ref3")));

  coordinator_.SignalNetworkRequestNeeded(notification1);
  coordinator_.SignalNetworkRequestNeeded(notification2);
  coordinator_.SignalNetworkRequestNeeded(notification3);
  coordinator_.SignalNetworkRequestNeeded(notification4);

  // Finishing notification1 should trigger notification4.
  EXPECT_CALL(mock_delegate_,
              OnCanSendNetworkRequest(OneTimeTokenNotificationMatches("ref4")));
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
              OnCanSendNetworkRequest(OneTimeTokenNotificationMatches("ref1")));
  EXPECT_CALL(mock_delegate_,
              OnCanSendNetworkRequest(OneTimeTokenNotificationMatches("ref2")));
  EXPECT_CALL(mock_delegate_,
              OnCanSendNetworkRequest(OneTimeTokenNotificationMatches("ref3")));

  coordinator_.SignalNetworkRequestNeeded(notification1);
  coordinator_.SignalNetworkRequestNeeded(notification2);
  coordinator_.SignalNetworkRequestNeeded(notification3);

  // notification4 is now pending.
  coordinator_.SignalNetworkRequestNeeded(notification4);

  // Signal notification4 again. It should still be de-duplicated.
  coordinator_.SignalNetworkRequestNeeded(notification4);

  // Finishing notification1 should trigger notification4 only once.
  EXPECT_CALL(mock_delegate_,
              OnCanSendNetworkRequest(OneTimeTokenNotificationMatches("ref4")))
      .Times(1);
  coordinator_.InformOfNetworkRequestFinished(notification1);
}

}  // namespace

}  // namespace one_time_tokens
