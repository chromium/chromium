// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/engines/broker/engine_client.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/chrome_cleaner/engines/common/engine_result_codes.h"
#include "chrome/chrome_cleaner/ipc/mojo_task_runner.h"
#include "chrome/chrome_cleaner/logging/mock_logging_service.h"
#include "chrome/chrome_cleaner/logging/proto/shared_data.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace chrome_cleaner {

namespace {

class EngineClientTest : public testing::Test {
 public:
  void SetResultCodeForTest(int result_code) {
    stored_result_code_for_test_ = result_code;
  }

  void TearDown() override {
    LoggingServiceAPI::SetInstanceForTesting(nullptr);
  }

 protected:
  int stored_result_code_for_test_ = 0;
};

MATCHER_P2(OperationStatusIs, operation, result_code, "") {
  return arg.operation() == operation && arg.result_code() == result_code;
}
}  // namespace

TEST_F(EngineClientTest, MaybeLogResultCode) {
  scoped_refptr<MojoTaskRunner> mojo_task_runner = MojoTaskRunner::Create();
  scoped_refptr<EngineClient> client_instance =
      chrome_cleaner::EngineClient::CreateEngineClient(
          Engine::UNKNOWN,
          base::BindRepeating(&EngineClientTest::SetResultCodeForTest,
                              base::Unretained(this)),
          /*connection_error_callback=*/base::NullCallback(), mojo_task_runner);

  client_instance->MaybeLogResultCode(EngineClient::Operation::StartScan,
                                      EngineResultCode::kSuccess);
  EXPECT_EQ(0x00090000, stored_result_code_for_test_);

  client_instance->MaybeLogResultCode(EngineClient::Operation::Finalize,
                                      EngineResultCode::kSuccess);
  EXPECT_EQ(0x000E0000, stored_result_code_for_test_);

  // The first unsuccessful operation should "stick" in the registry and not be
  // overwritten. However, all the operations should be written to the log.
  client_instance->MaybeLogResultCode(EngineClient::Operation::StartScan,
                                      EngineResultCode::kWrongState);
  EXPECT_EQ(0x00090003, stored_result_code_for_test_);

  client_instance->MaybeLogResultCode(EngineClient::Operation::Finalize,
                                      EngineResultCode::kSuccess);
  EXPECT_EQ(0x00090003, stored_result_code_for_test_);

  client_instance->MaybeLogResultCode(EngineClient::Operation::Finalize,
                                      EngineResultCode::kInvalidParameter);
  EXPECT_EQ(0x00090003, stored_result_code_for_test_);
}

}  // namespace chrome_cleaner
