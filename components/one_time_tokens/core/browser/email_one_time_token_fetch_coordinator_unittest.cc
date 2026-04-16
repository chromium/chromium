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
              (const EncryptedMessageReference& reference),
              (override));
};

class EmailOneTimeTokenFetchCoordinatorTest : public testing::Test {
 public:
  EmailOneTimeTokenFetchCoordinatorTest() : coordinator_(mock_delegate_) {}

 protected:
  MockDelegate mock_delegate_;
  EmailOneTimeTokenFetchCoordinator coordinator_;
};

// Tests that the coordinator signals the delegate when a request is needed.
// This confirms the current pass-through behavior.
TEST_F(EmailOneTimeTokenFetchCoordinatorTest, SignalNetworkRequestNeeded) {
  const EncryptedMessageReference reference("test_reference");
  EXPECT_CALL(mock_delegate_, OnCanSendNetworkRequest(testing::Eq(reference)));

  coordinator_.SignalNetworkRequestNeeded(reference);
}

}  // namespace

}  // namespace one_time_tokens
