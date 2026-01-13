// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/eula_dialog_linux.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/test/scoped_path_override.h"
#include "chrome/browser/first_run/first_run_internal.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/grit/branded_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

// This must match kEulaSentinelFile in chrome/installer/util/util_constants.cc
constexpr char kEulaSentinelFile[] = "EULA Accepted";

class EulaDialogTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(user_data_dir_.CreateUniqueTempDir());
    // Override the user data directory to point to our temp dir
    user_data_dir_override_ = std::make_unique<base::ScopedPathOverride>(
        chrome::DIR_USER_DATA, user_data_dir_.GetPath());
  }

 protected:
  base::ScopedTempDir user_data_dir_;
  std::unique_ptr<base::ScopedPathOverride> user_data_dir_override_;
};

TEST_F(EulaDialogTest, ShowEulaDialogWithSentinel) {
  // Create sentinel file
  base::FilePath sentinel = base::PathService::CheckedGet(chrome::DIR_USER_DATA)
                                .Append(kEulaSentinelFile);
  ASSERT_TRUE(base::WriteFile(sentinel, ""));

  EXPECT_TRUE(first_run::internal::ShowEulaDialog());
}

}  // namespace
