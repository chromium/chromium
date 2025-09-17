// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/unbuffered_file_writer.h"

#include <windows.h>

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>

#include "base/byte_count.h"
#include "base/containers/heap_array.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace installer {

namespace {

using base::test::ErrorIs;
using base::test::HasValue;
using base::test::ValueIs;

class UnbufferedFileWriterTest : public testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  const base::FilePath temp_dir() const { return temp_dir_.GetPath(); }

 private:
  base::ScopedTempDir temp_dir_;
};

// Tests that a file with no contents works.
TEST_F(UnbufferedFileWriterTest, EmptyFile) {
  base::FilePath path = temp_dir().Append(FILE_PATH_LITERAL("empty"));
  ASSERT_OK_AND_ASSIGN(UnbufferedFileWriter writer,
                       UnbufferedFileWriter::Create(path));
  ASSERT_THAT(writer.Commit(std::nullopt), HasValue());
  ASSERT_THAT(base::GetFileSize(path), testing::Optional(0LL));
}

// Tests a small file; smaller than a physical sector.
TEST_F(UnbufferedFileWriterTest, SmallFile) {
  base::FilePath path = temp_dir().Append(FILE_PATH_LITERAL("empty"));
  std::string_view contents = "hi mom";
  ASSERT_OK_AND_ASSIGN(UnbufferedFileWriter writer,
                       UnbufferedFileWriter::Create(path));
  writer.write_buffer().copy_prefix_from(base::as_byte_span(contents));
  writer.Advance(contents.size());
  ASSERT_THAT(writer.Commit(std::nullopt), HasValue());
  ASSERT_THAT(base::GetFileSize(path), testing::Optional(contents.size()));
  std::string result;
  ASSERT_PRED2(base::ReadFileToString, path, &result);
  ASSERT_EQ(contents, result);
}

// Tests writing to a file in multiple small chunks.
TEST_F(UnbufferedFileWriterTest, CommitChunks) {
  base::FilePath path = temp_dir().Append(FILE_PATH_LITERAL("chunked"));
  ASSERT_OK_AND_ASSIGN(UnbufferedFileWriter writer,
                       UnbufferedFileWriter::Create(path));

  // The write buffer will be the size of one sector.
  const size_t sector_size = writer.write_buffer().size();
  // Alignment guarantees that this will hold.
  const size_t number_count = sector_size / sizeof(uint64_t);
  ASSERT_EQ(number_count * sizeof(uint64_t), sector_size);

  // Write four sectors' worth of increasing integers.
  uint64_t index = 0;
  for (int i = 0; i < 4; ++i) {
    base::span write_buffer = writer.write_buffer();
    ASSERT_EQ(write_buffer.size(), sector_size);
    // SAFETY: The write buffer is large enough to hold `number_count` numbers.
    std::ranges::generate(
        UNSAFE_BUFFERS(base::span(
            reinterpret_cast<uint64_t*>(write_buffer.data()), number_count)),
        [&index] { return index++; });
    writer.Advance(sector_size);
    // The write buffer is now full.
    ASSERT_EQ(writer.write_buffer().size(), 0U);
    // Send it to the disk.
    ASSERT_THAT(writer.Checkpoint(), HasValue());
  }
  ASSERT_THAT(writer.Commit(std::nullopt), HasValue());
  ASSERT_THAT(base::GetFileSize(path), testing::Optional(4 * sector_size));
  std::string result;
  ASSERT_PRED2(base::ReadFileToString, path, &result);
  ASSERT_EQ(result.size(), 4 * sector_size);
  // Confirm that the data is all there, and all in the correct order.
  index = 0;
  ASSERT_TRUE(std::ranges::all_of(
      // SAFETY: The string buffer is large enough to hold the numbers.
      UNSAFE_BUFFERS(base::span(reinterpret_cast<uint64_t*>(
                                    base::as_writable_byte_span(result).data()),
                                result.size() / sizeof(uint64_t)),
                     [&index](auto& slot) { return slot == index++; })));
}

// Tests writing a very large file.
TEST_F(UnbufferedFileWriterTest, VeryLarge) {
  static constexpr auto kFileSize = base::GiB(2.333);
  base::FilePath path = temp_dir().Append(FILE_PATH_LITERAL("very_large"));
  ASSERT_OK_AND_ASSIGN(
      UnbufferedFileWriter writer,
      UnbufferedFileWriter::Create(path, kFileSize.InBytesUnsigned()));
  writer.Advance(kFileSize.InBytesUnsigned());
  ASSERT_THAT(writer.Commit(std::nullopt), HasValue());
  ASSERT_THAT(base::GetFileSize(path), testing::Optional(kFileSize.InBytes()));
}

// Tests that creation fails if the target file already exists.
TEST_F(UnbufferedFileWriterTest, FileExists) {
  base::FilePath path = temp_dir().Append(FILE_PATH_LITERAL("exists"));
  ASSERT_TRUE(base::WriteFile(path, std::string_view("hi mom")));
  base::HistogramTester histogram_tester;
  ASSERT_THAT(UnbufferedFileWriter::Create(path), ErrorIs(ERROR_FILE_EXISTS));
  histogram_tester.ExpectUniqueSample(
      "Setup.Install.UnbufferedFileWriter.Create.Error", ERROR_FILE_EXISTS, 1);
}

// Tests that the target file is deleted upon destruction of an instance without
// commit.
TEST_F(UnbufferedFileWriterTest, DeleteWithoutCommit) {
  base::FilePath path = temp_dir().Append(FILE_PATH_LITERAL("file"));

  WIN32_FILE_ATTRIBUTE_DATA data = {};
  {
    ASSERT_OK_AND_ASSIGN(UnbufferedFileWriter writer,
                         UnbufferedFileWriter::Create(path));
    // An attempt to check for the file will fail with ACCESS_DENIED because the
    // file is marked for deletion.
    ASSERT_FALSE(::GetFileAttributesEx(path.value().c_str(),
                                       GetFileExInfoStandard, &data));
    ASSERT_EQ(::GetLastError(), static_cast<DWORD>(ERROR_ACCESS_DENIED));
  }
  // Now that the file has been deleted, an attempt to get its attributes will
  // fail with FILE_NOT_FOUND.
  ASSERT_FALSE(::GetFileAttributesEx(path.value().c_str(),
                                     GetFileExInfoStandard, &data));
  ASSERT_EQ(::GetLastError(), static_cast<DWORD>(ERROR_FILE_NOT_FOUND));
}

}  // namespace

}  // namespace installer
