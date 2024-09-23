// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/obfuscation/core/utils.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_obfuscation {
namespace {

// Helper function to divide data in chunks of random sizes.
void ObfuscateTestDataInChunks(const std::vector<uint8_t>& test_data,
                               std::vector<uint8_t>& obfuscated_content) {
  std::vector<uint8_t> derived_key;
  std::vector<uint8_t> nonce_prefix;
  auto header = CreateHeader(&derived_key, &nonce_prefix);
  ASSERT_TRUE(header.has_value());
  obfuscated_content.insert(obfuscated_content.end(), header.value().begin(),
                            header.value().end());

  uint32_t counter = 0;
  for (size_t i = 0; i < test_data.size();) {
    // Generate a random chunk size between 1 and remaining data size.
    size_t remaining_data = test_data.size() - i;
    size_t chunk_size =
        base::RandInt(1, std::min(remaining_data, kMaxChunkSize));

    std::vector<uint8_t> chunk(test_data.begin() + i,
                               test_data.begin() + i + chunk_size);
    auto obfuscated_result =
        ObfuscateDataChunk(chunk, derived_key, nonce_prefix, counter++,
                           (i + chunk_size >= test_data.size()));
    ASSERT_TRUE(obfuscated_result.has_value());

    std::move(obfuscated_result.value().begin(),
              obfuscated_result.value().end(),
              std::back_inserter(obfuscated_content));

    i += chunk_size;
  }
}

}  // namespace

class ObfuscationUtilsTest
    : public ::testing::TestWithParam<std::tuple<bool, size_t>> {
 protected:
  ObfuscationUtilsTest() {
    feature_list_.InitWithFeatureState(kEnterpriseFileObfuscation,
                                       file_obfuscation_feature_enabled());
  }

  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  base::FilePath test_file_path() const {
    return temp_dir_.GetPath().AppendASCII("test_file.txt");
  }

  size_t test_data_size() const { return std::get<1>(GetParam()); }

  bool file_obfuscation_feature_enabled() { return std::get<0>(GetParam()); }

  base::test::ScopedFeatureList feature_list_;
  base::ScopedTempDir temp_dir_;
};

TEST_P(ObfuscationUtilsTest, ObfuscateAndDeobfuscateSingleDataChunk) {
  // Obfuscate the data chunk.
  std::vector<uint8_t> test_data = base::RandBytesAsVector(test_data_size());

  std::vector<uint8_t> derived_key;
  std::vector<uint8_t> nonce_prefix;
  auto header = CreateHeader(&derived_key, &nonce_prefix);
  uint32_t counter = 0;

  auto obfuscated_chunk = ObfuscateDataChunk(
      base::as_byte_span(test_data), derived_key, nonce_prefix, counter, true);

  if (!file_obfuscation_feature_enabled()) {
    ASSERT_EQ(obfuscated_chunk.error(), Error::kDisabled);
    ASSERT_EQ(header.error(), Error::kDisabled);
    return;
  }
  ASSERT_TRUE(header.has_value());
  ASSERT_TRUE(obfuscated_chunk.has_value());
  ASSERT_NE(obfuscated_chunk.value(), test_data);

  // Deobfuscate the data chunk.
  auto header_data = GetHeaderData(header.value());
  ASSERT_TRUE(header_data.has_value());

  auto chunk_size = GetObfuscatedChunkSize(obfuscated_chunk.value());

  if (test_data.size() > kMaxChunkSize) {
    ASSERT_EQ(chunk_size.error(), Error::kDeobfuscationFailed);
    return;
  }

  ASSERT_TRUE(chunk_size.has_value());
  EXPECT_EQ(chunk_size.value(), test_data.size() + kAuthTagSize);

  auto deobfuscated_chunk = DeobfuscateDataChunk(
      base::make_span(obfuscated_chunk.value())
          .subspan(kChunkSizePrefixSize, chunk_size.value()),
      header_data.value().first, header_data.value().second, counter, true);
  ASSERT_TRUE(deobfuscated_chunk.has_value());
  EXPECT_EQ(deobfuscated_chunk.value(), test_data);

  // Deobfuscation should fail when we modify the ciphertext.
  obfuscated_chunk.value()[kChunkSizePrefixSize] ^= 1;
  deobfuscated_chunk = DeobfuscateDataChunk(
      base::make_span(obfuscated_chunk.value())
          .subspan(kChunkSizePrefixSize, chunk_size.value()),
      header_data.value().first, header_data.value().second, counter, true);
  ASSERT_EQ(deobfuscated_chunk.error(), Error::kDeobfuscationFailed);
}

TEST_P(ObfuscationUtilsTest, DeobfuscateFileInPlace) {
  std::vector<uint8_t> test_data = base::RandBytesAsVector(test_data_size());
  ASSERT_TRUE(base::WriteFile(test_file_path(), test_data));

  int64_t original_size = 0;
  base::GetFileSize(test_file_path(), &original_size);

  auto result = DeobfuscateFileInPlace(test_file_path());

  if (!file_obfuscation_feature_enabled()) {
    ASSERT_EQ(result.error(), Error::kDisabled);
    return;
  }

  // Deobfuscating an unobfuscated file should fail.
  ASSERT_EQ(result.error(), original_size == 0 ? Error::kFileOperationError
                                               : Error::kDeobfuscationFailed);

  std::vector<uint8_t> obfuscated_content;

  ObfuscateTestDataInChunks(test_data, obfuscated_content);

  ASSERT_TRUE(base::WriteFile(test_file_path(), obfuscated_content));
  ASSERT_TRUE(DeobfuscateFileInPlace(test_file_path()).has_value());

  auto deobfuscated_content = base::ReadFileToBytes(test_file_path());
  ASSERT_TRUE(deobfuscated_content.has_value());
  EXPECT_EQ(deobfuscated_content.value(), test_data);

  // Get deobfuscated file size which should match original.
  int64_t deobfuscated_size = 0;
  ASSERT_TRUE(base::GetFileSize(test_file_path(), &deobfuscated_size));
  EXPECT_EQ(deobfuscated_size, original_size);

  // Deobfuscating to an invalid path should fail.
  base::FilePath invalid_path(
      test_file_path().InsertBeforeExtensionASCII("_invalid"));
  ASSERT_EQ(DeobfuscateFileInPlace(invalid_path).error(),
            Error::kFileOperationError);
}

TEST_P(ObfuscationUtilsTest, ObfuscateAndDeobfuscateVariableChunks) {
  if (!file_obfuscation_feature_enabled()) {
    GTEST_SKIP() << "File obfuscation feature is disabled.";
  }

  // Create test data.
  std::vector<uint8_t> test_data = base::RandBytesAsVector(test_data_size());

  // Obfuscate data in chunks of random sizes.
  std::vector<uint8_t> obfuscated_content;
  ObfuscateTestDataInChunks(test_data, obfuscated_content);

  // Deobfuscate chunk by chunk.
  std::vector<uint8_t> deobfuscated_content;
  size_t offset = kHeaderSize;
  uint32_t counter = 0;

  std::vector<uint8_t> header(obfuscated_content.begin(),
                              obfuscated_content.begin() + kHeaderSize);

  auto header_data = GetHeaderData(header);
  ASSERT_TRUE(header_data.has_value());

  while (offset < obfuscated_content.size()) {
    // Read chunk size
    auto chunk_size =
        GetObfuscatedChunkSize(base::make_span(obfuscated_content)
                                   .subspan(offset, kChunkSizePrefixSize));
    ASSERT_TRUE(chunk_size.has_value());
    offset += kChunkSizePrefixSize;

    // Deobfuscate chunk
    auto deobfuscated_chunk = DeobfuscateDataChunk(
        base::make_span(obfuscated_content).subspan(offset, chunk_size.value()),
        header_data.value().first, header_data.value().second, counter++,
        (offset + chunk_size.value() >= obfuscated_content.size()));
    ASSERT_TRUE(deobfuscated_chunk.has_value());

    std::move(deobfuscated_chunk.value().begin(),
              deobfuscated_chunk.value().end(),
              std::back_inserter(deobfuscated_content));

    offset += chunk_size.value();
  }

  // Compare deobfuscated content with original test data
  EXPECT_EQ(deobfuscated_content, test_data);
}

INSTANTIATE_TEST_SUITE_P(
    ObfuscationUtilsFeatureTest,
    ObfuscationUtilsTest,
    ::testing::Combine(
        ::testing::Bool(),  // File obfuscator feature enabled/disabled
        ::testing::Values(0,
                          10,
                          kMaxChunkSize + 1024,
                          kMaxChunkSize * 2 + 1024)));

}  // namespace enterprise_obfuscation
