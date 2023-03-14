// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <string>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/rand_util.h"
#include "chromecast/base/scoped_temp_file.h"
#include "chromecast/crash/linux/dummy_minidump_generator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {

TEST(DummyMinidumpGeneratorTest, GenerateFailsWithInvalidPath) {
  // Create directory in which to put minidump.
  base::ScopedTempDir minidump_dir;
  ASSERT_TRUE(minidump_dir.CreateUniqueTempDir());

  // Attempt to generate a minidump from an invalid path.
  DummyMinidumpGenerator generator("/path/does/not/exist/minidump.dmp");
  ASSERT_FALSE(generator.Generate(
      minidump_dir.GetPath().Append("minidump.dmp").value()));
}

TEST(DummyMinidumpGeneratorTest, GenerateSucceedsWithSmallSource) {
  // Create directory in which to put minidump.
  base::ScopedTempDir minidump_dir;
  ASSERT_TRUE(minidump_dir.CreateUniqueTempDir());

  // Create a fake minidump file.
  ScopedTempFile fake_minidump;
  const std::string data("Test contents of the minidump file.\n");
  ASSERT_TRUE(base::WriteFile(fake_minidump.path(), data));

  DummyMinidumpGenerator generator(fake_minidump.path().value());
  base::FilePath new_minidump = minidump_dir.GetPath().Append("minidump.dmp");
  EXPECT_TRUE(generator.Generate(new_minidump.value()));

  // Original file should not exist, and new file should contain original
  // contents.
  std::string copied_data;
  EXPECT_FALSE(base::PathExists(fake_minidump.path()));
  ASSERT_TRUE(base::PathExists(new_minidump));
  EXPECT_TRUE(base::ReadFileToString(new_minidump, &copied_data));
  EXPECT_EQ(data, copied_data);
}

TEST(DummyMinidumpGeneratorTest, GenerateSucceedsWithLargeSource) {
  // Create directory in which to put minidump.
  base::ScopedTempDir minidump_dir;
  ASSERT_TRUE(minidump_dir.CreateUniqueTempDir());

  // Create a large fake minidump file.
  ScopedTempFile fake_minidump;
  size_t str_len = 32768 * 10 + 1;
  const std::string data = base::RandBytesAsString(str_len);

  // Write the string to the file.
  ASSERT_TRUE(base::WriteFile(fake_minidump.path(), data));

  base::FilePath new_minidump = minidump_dir.GetPath().Append("minidump.dmp");
  DummyMinidumpGenerator generator(fake_minidump.path().value());
  ASSERT_TRUE(generator.Generate(new_minidump.value()));

  // Original file should not exist, and new file should contain original
  // contents.
  std::string copied_data;
  EXPECT_FALSE(base::PathExists(fake_minidump.path()));
  ASSERT_TRUE(base::PathExists(new_minidump));
  EXPECT_TRUE(base::ReadFileToString(new_minidump, &copied_data));
  EXPECT_EQ(data, copied_data);
}

}  // namespace chromecast
