// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/service/service_process_prefs.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class ServiceProcessPrefsTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    prefs_.reset(new ServiceProcessPrefs(
        temp_dir_.GetPath().AppendASCII("service_process_prefs.txt"),
        base::ThreadTaskRunnerHandle::Get().get()));
  }

  void TearDown() override { prefs_.reset(); }

  // The path to temporary directory used to contain the test operations.
  base::ScopedTempDir temp_dir_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<ServiceProcessPrefs> prefs_;
};

// Test ability to retrieve prefs
TEST_F(ServiceProcessPrefsTest, RetrievePrefs) {
  prefs_->SetBoolean("testb", true);
  prefs_->SetString("tests", "testvalue");
  prefs_->WritePrefs();
  base::RunLoop().RunUntilIdle();
  prefs_->SetBoolean("testb", false);         // overwrite
  prefs_->SetString("tests", std::string());  // overwrite
  prefs_->ReadPrefs();
  EXPECT_EQ(prefs_->GetBoolean("testb", false), true);
  EXPECT_EQ(prefs_->GetString("tests", std::string()), "testvalue");
}
