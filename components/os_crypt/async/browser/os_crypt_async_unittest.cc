// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/async/browser/os_crypt_async.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace os_crypt_async {

class OSCryptAsyncTest : public ::testing::Test {
 protected:
  OSCryptAsync factory_;

 private:
  base::test::TaskEnvironment task_environment_;
};

// This test verifies that GetInstanceAsync can correctly handle multiple queued
// requests for an instance for a slow init.
TEST_F(OSCryptAsyncTest, MultipleCalls) {
  size_t calls = 0;
  const size_t kExpectedCalls = 10;
  base::RunLoop run_loop;
  std::list<base::CallbackListSubscription> subs;
  for (size_t call = 0; call < kExpectedCalls; call++) {
    subs.push_back(factory_.GetInstance(base::BindLambdaForTesting(
        [&calls, &run_loop](Encryptor encryptor, bool success) {
          calls++;
          if (calls == kExpectedCalls) {
            run_loop.Quit();
          }
        })));
  }
  run_loop.Run();
  EXPECT_EQ(calls, kExpectedCalls);
}

}  // namespace os_crypt_async
