// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/one_time_tokens/one_time_token_log_handler.h"

#include "base/values.h"
#include "components/autofill/core/browser/logging/log_receiver.h"
#include "components/autofill/core/browser/logging/log_router.h"
#include "components/one_time_tokens/core/browser/one_time_token_log_sink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using ::testing::StrictMock;

class MockLogReceiver : public LogReceiver {
 public:
  MOCK_METHOD(void, LogEntry, (const base::DictValue&), (override));
};

// Tests that `CreateOneTimeTokenLogHandler` returns a null callback when given
// a null `LogRouter`.
TEST(OneTimeTokenLogHandlerTest, NullLogRouterReturnsNullCallback) {
  EXPECT_TRUE(CreateOneTimeTokenLogHandler(nullptr).is_null());
}

// Tests that `CreateOneTimeTokenLogHandler` creates a handler that forwards
// messages emitted to `OneTimeTokenLogSink` to `LogRouter`.
TEST(OneTimeTokenLogHandlerTest, ForwardsLogMessagesToLogRouter) {
  LogRouter log_router;
  StrictMock<MockLogReceiver> receiver;
  log_router.RegisterReceiver(&receiver);

  one_time_tokens::OneTimeTokenLogSink log_sink;
  base::CallbackListSubscription subscription =
      log_sink.AddLogHandler(CreateOneTimeTokenLogHandler(&log_router));

  EXPECT_CALL(receiver, LogEntry);
  LOG_OTT(&log_sink) << "Test OTP message";

  log_router.UnregisterReceiver(&receiver);
}

}  // namespace
}  // namespace autofill
