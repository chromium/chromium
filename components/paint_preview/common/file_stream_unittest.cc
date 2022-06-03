// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/common/file_stream.h"

#include "base/files/scoped_temp_dir.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace paint_preview {

// Test reading and writing from a file.
TEST(PaintPreviewFileStreamTest, TestWriteRead) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path = temp_dir.GetPath().AppendASCII("test_file");
  std::vector<uint8_t> test_data = {0, 1, 2, 3, 4, 5, 8, 9};

  base::File write_file(
      file_path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  FileWStream wstream(std::move(write_file));
  EXPECT_TRUE(wstream.write(test_data.data(), test_data.size()));
  wstream.flush();
  wstream.Close();
  EXPECT_EQ(wstream.bytesWritten(), test_data.size());
  EXPECT_EQ(wstream.ActualBytesWritten(), test_data.size());
  EXPECT_FALSE(wstream.DidWriteFail());
  base::File read_file(file_path, base::File::FLAG_OPEN |
                                      base::File::FLAG_READ |
                                      base::File::FLAG_EXCLUSIVE_READ);
  FileRStream rstream(std::move(read_file));
  EXPECT_FALSE(rstream.isAtEnd());
  std::vector<uint8_t> read_data(test_data.size(), 0xFF);
  EXPECT_EQ(rstream.read(read_data.data(), read_data.size()), read_data.size());
  EXPECT_THAT(read_data,
              testing::ElementsAreArray(test_data.begin(), test_data.end()));
  EXPECT_TRUE(rstream.isAtEnd());
}

// Test writing failure.
TEST(PaintPreviewFileStreamTest, TestWriteFail) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path = temp_dir.GetPath().AppendASCII("test_file");
  std::vector<uint8_t> test_data = {0, 1, 2, 3, 4, 5, 8, 9};

  base::File bad_write_file(
      file_path, base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_READ);
  FileWStream wstream(std::move(bad_write_file));
  EXPECT_FALSE(wstream.write(test_data.data(), test_data.size()));
  EXPECT_EQ(wstream.bytesWritten(), test_data.size());
  EXPECT_EQ(wstream.ActualBytesWritten(), 0U);
  EXPECT_TRUE(wstream.DidWriteFail());
}

// Test writing beyond max.
TEST(PaintPreviewFileStreamTest, TestWriteFailCapped) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path = temp_dir.GetPath().AppendASCII("test_file");
  std::vector<uint8_t> test_data = {0, 1, 2, 3, 4, 5, 8, 9};

  base::File write_file(
      file_path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  FileWStream wstream(std::move(write_file), test_data.size());
  EXPECT_TRUE(wstream.write(test_data.data(), test_data.size()));
  EXPECT_FALSE(wstream.DidWriteFail());
  EXPECT_EQ(wstream.bytesWritten(), test_data.size());
  EXPECT_EQ(wstream.ActualBytesWritten(), test_data.size());
  EXPECT_FALSE(wstream.write(test_data.data(), test_data.size()));
  EXPECT_EQ(wstream.bytesWritten(), test_data.size() * 2);
  EXPECT_EQ(wstream.ActualBytesWritten(), test_data.size());
  EXPECT_TRUE(wstream.DidWriteFail());
}

// Test writing beyond max on first write.
TEST(PaintPreviewFileStreamTest, TestWriteFailCappedFirstWrite) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path = temp_dir.GetPath().AppendASCII("test_file");
  std::vector<uint8_t> test_data = {0, 1, 2, 3, 4, 5, 8, 9};

  base::File write_file(
      file_path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  FileWStream wstream(std::move(write_file), 4);
  EXPECT_FALSE(wstream.write(test_data.data(), test_data.size()));
  EXPECT_TRUE(wstream.DidWriteFail());
}

// Test writing beyond max (with overflow).
TEST(PaintPreviewFileStreamTest, TestWriteFailOverflow) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path = temp_dir.GetPath().AppendASCII("test_file");
  std::vector<uint8_t> test_data = {0, 1, 2, 3, 4, 5, 8, 9};

  base::File write_file(
      file_path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  FileWStream wstream(std::move(write_file),
                      std::numeric_limits<size_t>::max());
  EXPECT_TRUE(wstream.write(test_data.data(), test_data.size()));
  EXPECT_FALSE(wstream.DidWriteFail());
  EXPECT_FALSE(
      wstream.write(test_data.data(), std::numeric_limits<size_t>::max()));
  EXPECT_TRUE(wstream.DidWriteFail());
}

// Test reading to skip.
TEST(PaintPreviewFileStreamTest, TestSkip) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path = temp_dir.GetPath().AppendASCII("test_file");
  std::vector<uint8_t> test_data = {0, 1, 2, 3, 4, 5, 8, 9};

  base::File write_file(
      file_path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  FileWStream wstream(std::move(write_file));
  // Write half the data.
  EXPECT_TRUE(wstream.write(test_data.data(), 4));
  EXPECT_EQ(wstream.bytesWritten(), 4U);
  EXPECT_TRUE(wstream.write(test_data.data() + 4, 4));
  EXPECT_EQ(wstream.bytesWritten(), test_data.size());
  wstream.Close();
  base::File read_file(file_path, base::File::FLAG_OPEN |
                                      base::File::FLAG_READ |
                                      base::File::FLAG_EXCLUSIVE_READ);
  FileRStream rstream(std::move(read_file));
  EXPECT_FALSE(rstream.isAtEnd());
  EXPECT_EQ(rstream.read(nullptr, test_data.size()), test_data.size());
  EXPECT_TRUE(rstream.isAtEnd());
}

// Test read and skip.
TEST(PaintPreviewFileStreamTest, TestReadAndSkip) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path = temp_dir.GetPath().AppendASCII("test_file");
  std::vector<uint8_t> test_data = {0, 1, 2, 3, 4, 5, 8, 9};

  base::File write_file(
      file_path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  FileWStream wstream(std::move(write_file));
  EXPECT_TRUE(wstream.write(test_data.data(), test_data.size()));
  wstream.Close();
  base::File read_file(file_path, base::File::FLAG_OPEN |
                                      base::File::FLAG_READ |
                                      base::File::FLAG_EXCLUSIVE_READ);
  const size_t kSkipBytes = 3;
  FileRStream rstream(std::move(read_file));
  EXPECT_FALSE(rstream.isAtEnd());
  EXPECT_EQ(rstream.read(nullptr, kSkipBytes), kSkipBytes);
  EXPECT_FALSE(rstream.isAtEnd());
  EXPECT_EQ(rstream.read(nullptr, kSkipBytes), kSkipBytes);
  EXPECT_FALSE(rstream.isAtEnd());
  const size_t kReadBytes = test_data.size() - kSkipBytes * 2;
  std::vector<uint8_t> read_data(kReadBytes, 0xFF);
  EXPECT_EQ(rstream.read(read_data.data(), read_data.size()), kReadBytes);
  EXPECT_THAT(read_data,
              testing::ElementsAreArray(test_data.begin() + kSkipBytes * 2,
                                        test_data.end()));
  EXPECT_TRUE(rstream.isAtEnd());
}

// Test reading failure.
TEST(PaintPreviewFileStreamTest, TestReadFail) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path = temp_dir.GetPath().AppendASCII("test_file");
  std::vector<uint8_t> test_data = {0, 1, 2, 3, 4, 5, 8, 9};

  base::File write_file(
      file_path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  FileWStream wstream(std::move(write_file));
  EXPECT_TRUE(wstream.write(test_data.data(), test_data.size()));
  base::File read_file(file_path,
                       base::File::FLAG_OPEN | base::File::FLAG_WRITE);
  FileRStream rstream(std::move(read_file));
  EXPECT_FALSE(rstream.isAtEnd());
  std::vector<uint8_t> read_data(test_data.size(), 0xFF);
  EXPECT_EQ(rstream.read(read_data.data(), read_data.size()), 0U);
  std::vector<uint8_t> expected(test_data.size(), 0xFF);
  EXPECT_THAT(read_data,
              testing::ElementsAreArray(expected.begin(), expected.end()));
}

}  // namespace paint_preview
