// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stdint.h>

#include <string>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/win/scoped_handle.h"
#include "chrome/installer/util/logging_installer.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(LoggingInstallerTest, TestTruncate) {
  const std::string test_data(installer::kMaxInstallerLogFileSize + 1, 'a');

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath temp_file = temp_dir.GetPath().Append(L"temp");
  EXPECT_TRUE(base::WriteFile(temp_file, test_data));
  ASSERT_TRUE(base::PathExists(temp_file));

  int64_t file_size = 0;
  EXPECT_TRUE(base::GetFileSize(temp_file, &file_size));
  EXPECT_EQ(static_cast<int64_t>(test_data.size()), file_size);

  EXPECT_EQ(installer::LOGFILE_TRUNCATED,
            installer::TruncateLogFileIfNeeded(temp_file));

  EXPECT_TRUE(base::GetFileSize(temp_file, &file_size));
  EXPECT_EQ(installer::kTruncatedInstallerLogFileSize, file_size);

  // Check that the temporary file was deleted.
  EXPECT_FALSE(base::PathExists(temp_file.Append(L".tmp")));
}

TEST(LoggingInstallerTest, TestTruncationNotNeeded) {
  const std::string test_data(installer::kMaxInstallerLogFileSize, 'a');

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath temp_file = temp_dir.GetPath().Append(L"temp");
  EXPECT_TRUE(base::WriteFile(temp_file, test_data));
  ASSERT_TRUE(base::PathExists(temp_file));

  int64_t file_size = 0;
  EXPECT_TRUE(base::GetFileSize(temp_file, &file_size));
  EXPECT_EQ(static_cast<int64_t>(test_data.size()), file_size);

  EXPECT_EQ(installer::LOGFILE_UNTOUCHED,
            installer::TruncateLogFileIfNeeded(temp_file));
  EXPECT_TRUE(base::PathExists(temp_file));
  EXPECT_TRUE(base::GetFileSize(temp_file, &file_size));
  EXPECT_EQ(static_cast<int64_t>(test_data.size()), file_size);
}

TEST(LoggingInstallerTest, TestInUseNeedsTruncation) {
  const std::string test_data(installer::kMaxInstallerLogFileSize + 1, 'a');

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath temp_file = temp_dir.GetPath().Append(L"temp");
  EXPECT_TRUE(base::WriteFile(temp_file, test_data));
  ASSERT_TRUE(base::PathExists(temp_file));
  int64_t file_size = 0;
  EXPECT_TRUE(base::GetFileSize(temp_file, &file_size));
  EXPECT_EQ(static_cast<int64_t>(test_data.size()), file_size);

  // Prevent the log file from being moved or deleted.
  uint32_t file_flags = base::File::FLAG_OPEN | base::File::FLAG_READ |
                        base::File::FLAG_WIN_EXCLUSIVE_READ;
  base::File temp_platform_file(temp_file, file_flags);
  ASSERT_TRUE(temp_platform_file.IsValid());

  EXPECT_EQ(installer::LOGFILE_UNTOUCHED,
            installer::TruncateLogFileIfNeeded(temp_file));
  EXPECT_TRUE(base::PathExists(temp_file));
  EXPECT_TRUE(base::GetFileSize(temp_file, &file_size));
  EXPECT_EQ(static_cast<int64_t>(test_data.size()), file_size);
}

TEST(LoggingInstallerTest, TestMoveFailsNeedsTruncation) {
  const std::string test_data(installer::kMaxInstallerLogFileSize + 1, 'a');

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath temp_file = temp_dir.GetPath().Append(L"temp");
  EXPECT_TRUE(base::WriteFile(temp_file, test_data));
  ASSERT_TRUE(base::PathExists(temp_file));
  int64_t file_size = 0;
  EXPECT_TRUE(base::GetFileSize(temp_file, &file_size));
  EXPECT_EQ(static_cast<int64_t>(test_data.size()), file_size);

  // Create an inconvenient, non-deletable file in the location that
  // TruncateLogFileIfNeeded would like to move the log file to.
  uint32_t file_flags = base::File::FLAG_CREATE | base::File::FLAG_READ |
                        base::File::FLAG_WIN_EXCLUSIVE_READ;
  base::FilePath temp_file_move_dest(temp_file.value() +
                                     FILE_PATH_LITERAL(".tmp"));
  base::File temp_move_destination_file(temp_file_move_dest, file_flags);
  ASSERT_TRUE(temp_move_destination_file.IsValid());

  EXPECT_EQ(installer::LOGFILE_DELETED,
            installer::TruncateLogFileIfNeeded(temp_file));
  EXPECT_FALSE(base::PathExists(temp_file));
}
