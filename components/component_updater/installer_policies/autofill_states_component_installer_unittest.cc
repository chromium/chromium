// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/installer_policies/autofill_states_component_installer.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {

class AutofillStatesComponentTest : public ::testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(user_data_dir_.CreateUniqueTempDir());
    filenames_ = {"US", "IN", "DE", "AB"};
  }

  const base::FilePath& user_data_dir() const {
    return user_data_dir_.GetPath();
  }

  void CreateEmptyFiles(const base::FilePath& dir) {
    for (const char* filename : filenames_) {
      base::WriteFile(dir.AppendASCII(filename), "");
    }
  }

 protected:
  base::test::TaskEnvironment env_;

 private:
  base::ScopedTempDir user_data_dir_;
  std::vector<const char*> filenames_;
};

// Tests that the component directory is deleted.
TEST_F(AutofillStatesComponentTest, DeleteComponent) {
  const base::FilePath component_dir =
      user_data_dir().Append(FILE_PATH_LITERAL("AutofillStates"));
  ASSERT_TRUE(base::CreateDirectory(component_dir));
  CreateEmptyFiles(component_dir);
  ASSERT_TRUE(base::PathExists(component_dir));

  DeleteAutofillStatesComponent(user_data_dir());

  env_.RunUntilIdle();
  EXPECT_TRUE(!base::PathExists(component_dir));
}

}  // namespace component_updater
