// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/sqlite/vfs/sandboxed_file.h"

#include "base/containers/span.h"
#include "base/files/scoped_temp_dir.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace persistent_cache {

namespace {

constexpr size_t kTestBufferLength = 1024;

class SandboxedFileTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temporary_directory_.CreateUniqueTempDir());
  }

  std::unique_ptr<SandboxedFile> CreateEmptyFile(const std::string& file_name) {
    return std::make_unique<SandboxedFile>(
        base::File(temporary_directory_.GetPath().AppendASCII(file_name),
                   base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_READ |
                       base::File::FLAG_WRITE),
        SandboxedFile::AccessRights::kReadWrite);
  }

  // Simulate an OpenFile from the VFS delegate.
  void OpenFile(SandboxedFile* file) {
    file->OnFileOpened(file->TakeUnderlyingFile());
  }

  int ReadToBuffer(SandboxedFile* file, size_t offset) {
    // Prepare the buffer used for readback.
    buffer_.resize(kTestBufferLength);
    std::fill(buffer_.begin(), buffer_.end(), 0xCD);

    // Read from the underlying file.
    auto buffer_as_span = base::span(buffer_);
    return file->Read(buffer_as_span.data(), buffer_as_span.size(), offset);
  }

  int WriteToFile(SandboxedFile* file,
                  size_t offset,
                  std::string_view content) {
    return file->Write(content.data(), content.size(), offset);
  }

  base::span<uint8_t> GetReadBuffer() { return base::span(buffer_); }

 private:
  base::ScopedTempDir temporary_directory_;
  std::vector<uint8_t> buffer_;
};

TEST_F(SandboxedFileTest, OpenClose) {
  std::unique_ptr<SandboxedFile> file = CreateEmptyFile("open");
  EXPECT_FALSE(file->IsValid());

  OpenFile(file.get());
  EXPECT_TRUE(file->IsValid());
  EXPECT_FALSE(file->TakeUnderlyingFile().IsValid());

  file->Close();
  EXPECT_FALSE(file->IsValid());
}

TEST_F(SandboxedFileTest, ReOpen) {
  std::unique_ptr<SandboxedFile> file = CreateEmptyFile("re-open");
  OpenFile(file.get());
  file->Close();

  // It is valid to re-open a file after a close.
  OpenFile(file.get());
  EXPECT_TRUE(file->IsValid());
  file->Close();
  EXPECT_FALSE(file->IsValid());
}

TEST_F(SandboxedFileTest, BasicReadWrite) {
  std::unique_ptr<SandboxedFile> file = CreateEmptyFile("basic");
  OpenFile(file.get());

  std::vector<uint8_t> content(kTestBufferLength, 0xCA);
  EXPECT_EQ(file->Write(content.data(), content.size(), 0), SQLITE_OK);

  // Read back data.
  EXPECT_EQ(ReadToBuffer(file.get(), 0), SQLITE_OK);
  EXPECT_EQ(GetReadBuffer(), base::span(content));
}

TEST_F(SandboxedFileTest, ReadToShort) {
  std::unique_ptr<SandboxedFile> file = CreateEmptyFile("short");
  OpenFile(file.get());

  const std::string content = "This is a short text";
  EXPECT_EQ(WriteToFile(file.get(), 0, content), SQLITE_OK);

  // Read back data. Read(..) must fill the buffer with zeroes.
  EXPECT_EQ(ReadToBuffer(file.get(), 0), SQLITE_IOERR_SHORT_READ);

  // Build the expected buffer with the trailing zeroes.
  std::vector<uint8_t> expected_buffer(kTestBufferLength, 0);
  auto expected_buffer_as_span = base::span(expected_buffer);
  std::copy(content.begin(), content.end(), expected_buffer_as_span.begin());

  EXPECT_EQ(GetReadBuffer(), expected_buffer_as_span);
}

TEST_F(SandboxedFileTest, ReadTooFar) {
  std::unique_ptr<SandboxedFile> file = CreateEmptyFile("too-short");
  OpenFile(file.get());

  const std::string content = "This is a too short text";
  EXPECT_EQ(WriteToFile(file.get(), 0, content), SQLITE_OK);

  // SQLite itself does not treat reading beyond the end of the file as an
  // error.
  constexpr size_t kTooFarOffset = 0x100000;
  EXPECT_EQ(ReadToBuffer(file.get(), kTooFarOffset), SQLITE_IOERR_SHORT_READ);

  // Build the expected buffer. A buffer full of zeroes.
  std::vector<uint8_t> expected_buffer(kTestBufferLength, 0);

  EXPECT_EQ(GetReadBuffer(), base::span(expected_buffer));
}

TEST_F(SandboxedFileTest, ReadWithOffset) {
  std::unique_ptr<SandboxedFile> file = CreateEmptyFile("offset");
  OpenFile(file.get());

  const std::string content = "The answer is 42";
  EXPECT_EQ(WriteToFile(file.get(), 0, content), SQLITE_OK);

  // Read back data. Read(..) must fill the buffer with zeroes.
  const size_t kReadOffset = content.find("42");
  EXPECT_EQ(ReadToBuffer(file.get(), kReadOffset), SQLITE_IOERR_SHORT_READ);

  // Build the expected buffer with the trailing zeroes.
  std::vector<uint8_t> expected_buffer(kTestBufferLength, 0);
  auto content_at_offset = base::byte_span_with_nul_from_cstring("42");
  std::copy(content_at_offset.begin(), content_at_offset.end(),
            expected_buffer.begin());

  EXPECT_EQ(GetReadBuffer(), base::span(expected_buffer));
}

TEST_F(SandboxedFileTest, WriteWithOffset) {
  std::unique_ptr<SandboxedFile> file = CreateEmptyFile("offset");
  OpenFile(file.get());

  // Write pass end-of-file should increase the file size and fill the gab with
  // zeroes.
  const std::string content = "The answer is 42";
  constexpr size_t kWriteOffset = 42;
  EXPECT_EQ(WriteToFile(file.get(), kWriteOffset, content), SQLITE_OK);

  // Read back data. Read(..) must fill the buffer with zeroes.
  EXPECT_EQ(ReadToBuffer(file.get(), 0), SQLITE_IOERR_SHORT_READ);

  // Build the expected buffer with the trailing zeroes.
  std::vector<uint8_t> expected_buffer(kTestBufferLength, 0);
  auto expected_buffer_as_span = base::span(expected_buffer);
  std::copy(content.begin(), content.end(),
            expected_buffer_as_span.subspan(kWriteOffset).begin());

  EXPECT_EQ(GetReadBuffer(), base::span(expected_buffer));
}

TEST_F(SandboxedFileTest, OverlappingWrites) {
  std::unique_ptr<SandboxedFile> file = CreateEmptyFile("writes");
  OpenFile(file.get());

  const std::string content1 = "aaa";
  const std::string content2 = "bbb";
  const std::string content3 = "ccc";

  const size_t kWriteOffset1 = 0;
  const size_t kWriteOffset2 = 4;
  const size_t kWriteOffset3 = 2;

  EXPECT_EQ(WriteToFile(file.get(), kWriteOffset1, content1), SQLITE_OK);
  EXPECT_EQ(WriteToFile(file.get(), kWriteOffset2, content2), SQLITE_OK);
  EXPECT_EQ(WriteToFile(file.get(), kWriteOffset3, content3), SQLITE_OK);

  // Read back data.
  EXPECT_EQ(ReadToBuffer(file.get(), 0), SQLITE_IOERR_SHORT_READ);

  // Build the expected buffer with the trailing zeroes.
  std::vector<uint8_t> expected_buffer(kTestBufferLength, 0);
  auto expected_text = base::byte_span_with_nul_from_cstring("aacccbb");
  std::copy(expected_text.begin(), expected_text.end(),
            expected_buffer.begin());

  EXPECT_EQ(GetReadBuffer(), base::span(expected_buffer));
}

TEST_F(SandboxedFileTest, Truncate) {
  std::unique_ptr<SandboxedFile> file = CreateEmptyFile("truncate");
  OpenFile(file.get());

  std::vector<uint8_t> content(kTestBufferLength, 0xCA);
  EXPECT_EQ(file->Write(content.data(), content.size(), 0), SQLITE_OK);

  // Validate filesize before truncate.
  sqlite3_int64 file_size = 0;
  EXPECT_EQ(file->FileSize(&file_size), SQLITE_OK);
  EXPECT_EQ(static_cast<size_t>(file_size), kTestBufferLength);

  // Truncate the content of the file.
  constexpr size_t kTruncateLength = 10;
  file->Truncate(kTruncateLength);

  // Ensure the filesize changed after truncate.
  EXPECT_EQ(file->FileSize(&file_size), SQLITE_OK);
  EXPECT_EQ(static_cast<size_t>(file_size), kTruncateLength);

  // Read back data.
  EXPECT_EQ(ReadToBuffer(file.get(), 0), SQLITE_IOERR_SHORT_READ);

  // Build the expected buffer with the trailing zeroes.
  std::vector<uint8_t> expected_buffer(kTestBufferLength, 0);
  auto content_as_span = base::span(content);
  std::copy(content_as_span.begin(), content_as_span.begin() + kTruncateLength,
            expected_buffer.begin());

  EXPECT_EQ(GetReadBuffer(), base::span(expected_buffer));
}

}  // namespace

}  // namespace persistent_cache
