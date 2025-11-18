// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/obfuscation/core/obfuscated_file_reader.h"

#include <numeric>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/enterprise/obfuscation/core/download_obfuscator.h"
#include "components/enterprise/obfuscation/core/utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_obfuscation {

namespace {
constexpr std::string_view kTestData1 = "Hello, world!";
constexpr std::string_view kTestData2 = "This is another test.";
}  // namespace

class ObfuscatedFileReaderPeer {
 public:
  using ChunkInfo = ObfuscatedFileReader::ChunkInfo;

  static const std::vector<ChunkInfo>& GetChunkInfo(
      const ObfuscatedFileReader& reader) {
    return reader.chunk_info_;
  }
};

class ObfuscatedFileReaderTest : public testing::Test {
 public:
  ObfuscatedFileReaderTest() = default;

  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

 protected:
  // Returns a pair of <Obfuscated File, Cleartext Data>
  std::pair<base::File, std::vector<uint8_t>> CreateObfuscatedFile(
      const std::vector<std::string_view>& chunks) {
    enterprise_obfuscation::DownloadObfuscator obfuscator;
    std::vector<uint8_t> obfuscated_content;
    std::vector<uint8_t> cleartext_content;
    for (size_t i = 0; i < chunks.size(); ++i) {
      bool is_last_chunk = (i == chunks.size() - 1);
      base::span<const uint8_t> chunk_span = base::as_byte_span(chunks[i]);
      auto result = obfuscator.ObfuscateChunk(chunk_span, is_last_chunk);
      EXPECT_TRUE(result.has_value());
      obfuscated_content.insert(obfuscated_content.end(), result->begin(),
                                result->end());
      cleartext_content.insert(cleartext_content.end(), chunk_span.begin(),
                               chunk_span.end());
    }

    base::FilePath path = temp_dir_.GetPath().AppendASCII("obfuscated.bin");
    EXPECT_TRUE(base::WriteFile(path, obfuscated_content));

    return std::make_pair(
        base::File(path, base::File::FLAG_OPEN | base::File::FLAG_READ),
        cleartext_content);
  }

  void RunSeekReadTest(ObfuscatedFileReader& reader,
                       const std::vector<uint8_t>& cleartext) {
    if (cleartext.empty()) {
      return;
    }

    std::vector<size_t> positions;
    const int size_limit = 10'000;
    if (cleartext.size() <= size_limit) {
      positions.resize(cleartext.size());
      std::iota(positions.begin(), positions.end(), 0);
    } else {
      positions.reserve(size_limit * 2);
      for (size_t i = 0; i < size_limit; ++i) {
        positions.push_back(i);
      }
      for (size_t i = 0; i < size_limit; ++i) {
        positions.push_back(cleartext.size() - size_limit + i);
      }
    }

    for (size_t pos : positions) {
      int64_t new_offset = reader.Seek(pos, base::File::Whence::FROM_BEGIN);
      EXPECT_EQ(new_offset, static_cast<int64_t>(pos));

      int64_t current_offset = reader.Tell();
      EXPECT_EQ(current_offset, static_cast<int64_t>(pos));

      size_t bytes_to_read = std::min<size_t>(10, cleartext.size() - pos);
      if (bytes_to_read == 0) {
        continue;
      }

      std::vector<uint8_t> buffer(bytes_to_read);
      int64_t result = reader.Read(buffer);

      ASSERT_EQ(result, static_cast<int64_t>(bytes_to_read));

      auto expected_span = base::span(cleartext).subspan(pos, bytes_to_read);
      EXPECT_EQ(buffer, expected_span);

      current_offset = reader.Tell();
      EXPECT_EQ(current_offset, static_cast<int64_t>(pos + bytes_to_read));
    }
  }

  void VerifyChunkIndex(
      const std::vector<ObfuscatedFileReaderPeer::ChunkInfo>& chunk_info,
      const std::vector<uint64_t>& expected_chunk_sizes) {
    ASSERT_EQ(chunk_info.size(), expected_chunk_sizes.size());

    uint64_t total_deobfuscated_size = 0;
    uint64_t expected_obfuscated_offset = kHeaderSize + kChunkSizePrefixSize;

    for (size_t i = 0; i < expected_chunk_sizes.size(); ++i) {
      uint64_t expected_deobfuscated_size = expected_chunk_sizes[i];
      EXPECT_EQ(chunk_info[i].obfuscated_offset, expected_obfuscated_offset);
      EXPECT_EQ(chunk_info[i].deobfuscated_size, expected_deobfuscated_size);
      EXPECT_EQ(chunk_info[i].deobfuscated_offset, total_deobfuscated_size);

      total_deobfuscated_size += expected_deobfuscated_size;
      expected_obfuscated_offset +=
          kChunkSizePrefixSize + expected_deobfuscated_size + kAuthTagSize;
    }
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
};

class ObfuscatedFileReaderParamTest
    : public ObfuscatedFileReaderTest,
      public testing::WithParamInterface<std::vector<std::string_view>> {};

TEST_P(ObfuscatedFileReaderParamTest, BuildChunkIndex) {
  const auto& chunks = GetParam();
  auto [file, cleartext] = CreateObfuscatedFile(chunks);
  ASSERT_TRUE(file.IsValid());

  auto header_data = ObfuscatedFileReader::ReadHeaderData(file);
  ASSERT_TRUE(header_data.has_value());

  auto reader_result = ObfuscatedFileReader::Create(
      std::move(header_data).value(), std::move(file));
  ASSERT_TRUE(reader_result.has_value());
  ObfuscatedFileReader& reader = reader_result.value();

  const auto& chunk_info = ObfuscatedFileReaderPeer::GetChunkInfo(reader);
  ASSERT_EQ(chunk_info.size(), chunks.size());

  std::vector<uint64_t> expected_chunk_sizes;
  for (const auto& chunk : chunks) {
    expected_chunk_sizes.push_back(chunk.size());
  }
  VerifyChunkIndex(chunk_info, expected_chunk_sizes);

  uint64_t total_deobfuscated_size = 0;
  for (const auto& chunk : chunks) {
    total_deobfuscated_size += chunk.size();
  }

  EXPECT_EQ(reader.GetSize(), static_cast<int64_t>(total_deobfuscated_size));

  RunSeekReadTest(reader, cleartext);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ObfuscatedFileReaderParamTest,
    testing::Values(std::vector<std::string_view>{kTestData1},
                    std::vector<std::string_view>{kTestData1, kTestData2}));

TEST_F(ObfuscatedFileReaderTest, BuildChunkIndexLargeFile) {
  constexpr size_t kTenMB = 10 * 1024 * 1024;
  constexpr size_t kChunkSize = 1000;
  std::vector<uint8_t> large_data(kTenMB);
  for (size_t i = 0; i < kTenMB; ++i) {
    large_data[i] = static_cast<uint8_t>(i % 256);
  }

  enterprise_obfuscation::DownloadObfuscator obfuscator;
  std::vector<uint8_t> obfuscated_content;
  base::span<const uint8_t> full_span = large_data;
  size_t num_chunks = 0;
  for (size_t offset = 0; offset < kTenMB; offset += kChunkSize) {
    size_t current_chunk_size = std::min(kChunkSize, kTenMB - offset);
    base::span<const uint8_t> chunk_span =
        full_span.subspan(offset, current_chunk_size);
    bool is_last_chunk = (offset + current_chunk_size == kTenMB);
    auto result = obfuscator.ObfuscateChunk(chunk_span, is_last_chunk);
    ASSERT_TRUE(result.has_value());
    obfuscated_content.insert(obfuscated_content.end(), result->begin(),
                              result->end());
    num_chunks++;
  }

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath path = temp_dir.GetPath().AppendASCII("obfuscated_large.bin");
  ASSERT_TRUE(base::WriteFile(path, obfuscated_content));

  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(file.IsValid());

  auto header_data = ObfuscatedFileReader::ReadHeaderData(file);
  ASSERT_TRUE(header_data.has_value());

  auto reader_result = ObfuscatedFileReader::Create(
      std::move(header_data).value(), std::move(file));
  ASSERT_TRUE(reader_result.has_value());
  ObfuscatedFileReader& reader = reader_result.value();

  const auto& chunk_info = ObfuscatedFileReaderPeer::GetChunkInfo(reader);
  EXPECT_EQ(chunk_info.size(), num_chunks);
  EXPECT_EQ(static_cast<size_t>(reader.GetSize()), kTenMB);

  std::vector<uint64_t> expected_chunk_sizes;
  for (size_t offset = 0; offset < kTenMB; offset += kChunkSize) {
    expected_chunk_sizes.push_back(std::min(kChunkSize, kTenMB - offset));
  }
  VerifyChunkIndex(chunk_info, expected_chunk_sizes);

  EXPECT_EQ(reader.GetSize(), static_cast<int64_t>(kTenMB));

  RunSeekReadTest(reader, large_data);
}

TEST_F(ObfuscatedFileReaderTest, SeekModes) {
  auto [file, cleartext] = CreateObfuscatedFile({kTestData1, kTestData2});
  ASSERT_TRUE(file.IsValid());

  auto header_data = ObfuscatedFileReader::ReadHeaderData(file);
  ASSERT_TRUE(header_data.has_value());

  auto reader_result = ObfuscatedFileReader::Create(
      std::move(header_data).value(), std::move(file));
  ASSERT_TRUE(reader_result.has_value());
  ObfuscatedFileReader& reader = reader_result.value();

  const int64_t file_size = static_cast<int64_t>(cleartext.size());
  ASSERT_EQ(reader.GetSize(), file_size);

  auto do_seek = [&](int64_t offset, base::File::Whence whence,
                     int64_t expected_offset) {
    int64_t new_offset = reader.Seek(offset, whence);
    EXPECT_EQ(new_offset, expected_offset);

    if (expected_offset != -1) {
      int64_t tell_offset = reader.Tell();
      EXPECT_EQ(tell_offset, expected_offset);
    }
  };

  // FROM_BEGIN
  do_seek(0, base::File::Whence::FROM_BEGIN, 0);
  do_seek(file_size / 2, base::File::Whence::FROM_BEGIN, file_size / 2);
  do_seek(file_size, base::File::Whence::FROM_BEGIN, file_size);
  do_seek(-1, base::File::Whence::FROM_BEGIN, -1);
  do_seek(file_size + 1, base::File::Whence::FROM_BEGIN, -1);

  // FROM_CURRENT
  do_seek(0, base::File::Whence::FROM_BEGIN, 0);  // Reset to start
  do_seek(10, base::File::Whence::FROM_CURRENT, 10);
  EXPECT_EQ(reader.Tell(), 10);
  do_seek(-5, base::File::Whence::FROM_CURRENT, 5);
  EXPECT_EQ(reader.Tell(), 5);
  do_seek(file_size - 5, base::File::Whence::FROM_CURRENT, file_size);
  EXPECT_EQ(reader.Tell(), file_size);
  do_seek(1, base::File::Whence::FROM_CURRENT, -1);  // Past end
  EXPECT_EQ(reader.Tell(), file_size);            // Should not change on error
  do_seek(0, base::File::Whence::FROM_BEGIN, 0);  // Reset to start
  do_seek(-1, base::File::Whence::FROM_CURRENT, -1);  // Before start
  EXPECT_EQ(reader.Tell(), 0);  // Should not change on error

  // FROM_END
  do_seek(0, base::File::Whence::FROM_END, file_size);
  do_seek(-file_size / 2, base::File::Whence::FROM_END,
          file_size - file_size / 2);
  do_seek(-file_size, base::File::Whence::FROM_END, 0);
  do_seek(1, base::File::Whence::FROM_END, -1);
  do_seek(-file_size - 1, base::File::Whence::FROM_END, -1);
}

TEST_F(ObfuscatedFileReaderTest, CorruptedFileTests) {
  // Test Case 1: Invalid Header Size (Too Small)
  {
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
    base::FilePath path =
        temp_dir.GetPath().AppendASCII("corrupt_header_size.bin");
    std::vector<uint8_t> content(kHeaderSize - 1, 0xAA);
    ASSERT_TRUE(base::WriteFile(path, content));

    base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
    ASSERT_TRUE(file.IsValid());
    auto header_data = ObfuscatedFileReader::ReadHeaderData(file);
    EXPECT_FALSE(header_data.has_value());
    EXPECT_EQ(header_data.error(), Error::kFileOperationError);
  }

  // Test Case 2: Invalid Header Data (Malformed)
  {
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
    base::FilePath path =
        temp_dir.GetPath().AppendASCII("corrupt_header_data.bin");
    std::vector<uint8_t> content(kHeaderSize, 0xBB);
    // Intentionally corrupt the header size field.
    content[0] = 0;  // Invalid header size
    ASSERT_TRUE(base::WriteFile(path, content));

    base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
    ASSERT_TRUE(file.IsValid());
    auto header_data = ObfuscatedFileReader::ReadHeaderData(file);
    EXPECT_FALSE(header_data.has_value());
    EXPECT_EQ(header_data.error(), Error::kDeobfuscationFailed);
  }

  // Test Case 3: File Too Small for Any Chunks
  {
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
    base::FilePath path =
        temp_dir.GetPath().AppendASCII("corrupt_too_small.bin");
    std::vector<uint8_t> content(kHeaderSize + kChunkSizePrefixSize - 1, 0xCC);
    ASSERT_TRUE(base::WriteFile(path, content));

    // Valid header for this test.
    std::array<uint8_t, kKeySize> derived_key;
    std::vector<uint8_t> nonce_prefix;
    auto header = CreateHeader(&derived_key, &nonce_prefix);
    ASSERT_TRUE(header.has_value());
    std::copy(header->begin(), header->end(), content.begin());
    ASSERT_TRUE(base::WriteFile(path, content));

    base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
    ASSERT_TRUE(file.IsValid());
    auto header_data = ObfuscatedFileReader::ReadHeaderData(file);
    ASSERT_TRUE(header_data.has_value());

    auto reader_result = ObfuscatedFileReader::Create(
        std::move(header_data).value(), std::move(file));
    EXPECT_FALSE(reader_result.has_value());
    EXPECT_EQ(reader_result.error(), Error::kDeobfuscationFailed);
  }

  // Test Case 4: Incomplete Chunk Size Prefix
  {
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
    base::FilePath path =
        temp_dir.GetPath().AppendASCII("incomplete_chunk.bin");

    enterprise_obfuscation::DownloadObfuscator obfuscator;
    std::vector<uint8_t> obfuscated_content;
    base::span<const uint8_t> chunk_span = base::as_byte_span(kTestData1);
    auto result = obfuscator.ObfuscateChunk(chunk_span, true);
    ASSERT_TRUE(result.has_value());
    obfuscated_content.insert(obfuscated_content.end(), result->begin(),
                              result->end());
    ASSERT_TRUE(base::WriteFile(path, obfuscated_content));

    base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                              base::File::FLAG_WRITE);
    ASSERT_TRUE(file.IsValid());

    int64_t file_size = file.GetLength();
    ASSERT_GT(static_cast<size_t>(file_size),
              kHeaderSize + kChunkSizePrefixSize);

    // Truncate the file to have an incomplete chunk size prefix.
    ASSERT_TRUE(file.SetLength(file_size - 1));
    file.Close();  // Close the file to release the handle.

    base::File read_file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
    ASSERT_TRUE(read_file.IsValid());

    auto header_data = ObfuscatedFileReader::ReadHeaderData(read_file);
    ASSERT_TRUE(header_data.has_value());

    auto reader_result = ObfuscatedFileReader::Create(
        std::move(header_data).value(), std::move(read_file));
    EXPECT_FALSE(reader_result.has_value());
    EXPECT_EQ(reader_result.error(), Error::kDeobfuscationFailed);
  }

  // Test Case 5: Deobfuscation Failed for a Chunk (Tampered Data)
  {
    enterprise_obfuscation::DownloadObfuscator obfuscator;
    base::span<const uint8_t> chunk_span = base::as_byte_span(kTestData1);
    auto result = obfuscator.ObfuscateChunk(chunk_span, true);
    ASSERT_TRUE(result.has_value());
    std::vector<uint8_t> obfuscated_content = std::move(result).value();

    // Tamper with the obfuscated data.
    obfuscated_content[obfuscated_content.size() - kAuthTagSize - 1] ^= 0xFF;

    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
    base::FilePath path = temp_dir.GetPath().AppendASCII("corrupt_chunk.bin");
    ASSERT_TRUE(base::WriteFile(path, obfuscated_content));

    base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
    ASSERT_TRUE(file.IsValid());

    auto header_data = ObfuscatedFileReader::ReadHeaderData(file);
    ASSERT_TRUE(header_data.has_value());

    auto reader_result = ObfuscatedFileReader::Create(
        std::move(header_data).value(), std::move(file));
    ASSERT_TRUE(reader_result.has_value());
    ObfuscatedFileReader& reader = reader_result.value();

    std::vector<uint8_t> buffer(kTestData1.size());
    int64_t read_result = reader.Read(buffer);
    EXPECT_EQ(read_result, -1);
  }
}

}  // namespace enterprise_obfuscation
