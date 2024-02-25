// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/logging/log_buffer_submitter.h"

#include <tuple>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/values.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/logging/log_receiver.h"
#include "components/autofill/core/browser/logging/log_router.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest-death-test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class MockLogReceiver : public LogReceiver {
 public:
  MOCK_METHOD(void, LogEntry, (const base::Value::Dict&), (override));
};

TEST(LogBufferSubmitter, VerifySubmissionOnDestruction) {
  LogBuffer buffer;
  buffer << 42;
  std::optional<base::Value::Dict> expected = buffer.RetrieveResult();

  MockLogReceiver receiver;
  LogRouter router;
  router.RegisterReceiver(&receiver);
  std::unique_ptr<LogManager> log_manager =
      LogManager::Create(&router, base::NullCallback());

  EXPECT_CALL(receiver, LogEntry(testing::Eq(testing::ByRef(*expected))));
  log_manager->Log() << 42;
  log_manager.reset();
  router.UnregisterReceiver(&receiver);
}

TEST(LogBufferSubmitter, NoEmptySubmission) {
  MockLogReceiver receiver;
  LogRouter router;
  router.RegisterReceiver(&receiver);
  std::unique_ptr<LogManager> log_manager =
      LogManager::Create(&router, base::NullCallback());

  EXPECT_CALL(receiver, LogEntry(testing::_)).Times(0);
  log_manager->Log();
  log_manager.reset();
  router.UnregisterReceiver(&receiver);
}

TEST(LogBufferSubmitter, CorrectActivation) {
  std::unique_ptr<LogManager> log_manager =
      LogManager::Create(nullptr, base::NullCallback());
  EXPECT_FALSE(log_manager->Log().buffer().active());

  LogRouter router;
  MockLogReceiver receiver;
  router.RegisterReceiver(&receiver);
  std::unique_ptr<LogManager> log_manager_2 =
      LogManager::Create(&router, base::NullCallback());
  EXPECT_TRUE(log_manager_2->Log().buffer().active());
  router.UnregisterReceiver(&receiver);
}

}  // namespace autofill
