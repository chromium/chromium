// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/file_util/public/cpp/temporary_file_getter.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class TemporaryFileGetterTest : public ::testing::Test {
 public:
  TemporaryFileGetterTest();

  content::BrowserTaskEnvironment task_environment_;
  base::File temp_file_;
  TemporaryFileGetter temp_file_getter_;
};

void UpdateTempFile(base::File* temp_file_, base::File temp_file) {
  *temp_file_ = std::move(temp_file);
}

TemporaryFileGetterTest::TemporaryFileGetterTest() = default;

TEST_F(TemporaryFileGetterTest, GetTempFileTest) {
  auto callback = base::BindOnce(&UpdateTempFile, &temp_file_);
  temp_file_getter_.RequestTemporaryFile(std::move(callback));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(temp_file_.IsValid());
}

TEST_F(TemporaryFileGetterTest, ExceedFileLimitTest) {
  // Get the allowed 10 files from the TemporaryFileGetter.
  for (int i = 0; i < 10; i++) {
    auto callback = base::BindOnce(&UpdateTempFile, &temp_file_);
    temp_file_getter_.RequestTemporaryFile(std::move(callback));
    task_environment_.RunUntilIdle();
    EXPECT_TRUE(temp_file_.IsValid());
  }
  // After 10 files, the TemporaryFileGetter returns an invalid file.
  auto callback = base::BindOnce(&UpdateTempFile, &temp_file_);
  temp_file_getter_.RequestTemporaryFile(std::move(callback));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(temp_file_.IsValid());
}

}  // namespace safe_browsing
