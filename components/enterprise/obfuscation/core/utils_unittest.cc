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

constexpr size_t kKeySize = 32u;
constexpr size_t kDataChunkSize = 524288;  // default download buffer size
constexpr size_t kAuthTagSize = 16u;
constexpr size_t kNoncePrefixSize = 7u;
constexpr size_t kHeaderSize = 1u + kKeySize + kNoncePrefixSize;

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

TEST_P(ObfuscationUtilsTest, ObfuscateAndDeobfuscateDataChunk) {
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

  auto deobfuscated_chunk =
      DeobfuscateDataChunk(obfuscated_chunk.value(), header_data.value().first,
                           header_data.value().second, counter, true);
  ASSERT_TRUE(deobfuscated_chunk.has_value());
  EXPECT_EQ(deobfuscated_chunk.value(), test_data);

  // Deobfuscation should fail when we modify the ciphertext.
  obfuscated_chunk.value()[0] ^= 1;
  deobfuscated_chunk =
      DeobfuscateDataChunk(obfuscated_chunk.value(), header_data.value().first,
                           header_data.value().second, counter, true);
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

  // Calculate the number of chunks and reserve needed space.
  size_t num_chunks = (test_data.size() + kDataChunkSize - 1) / kDataChunkSize;
  obfuscated_content.reserve(test_data.size() + num_chunks * kAuthTagSize +
                             kHeaderSize);

  std::vector<uint8_t> derived_key;
  std::vector<uint8_t> nonce_prefix;
  auto header = CreateHeader(&derived_key, &nonce_prefix);
  obfuscated_content.insert(obfuscated_content.end(), header.value().begin(),
                            header.value().end());

  // Obufscate in chunks of kChunkSize if the content is large
  uint32_t counter = 0;
  for (size_t i = 0; i < test_data.size(); i += kDataChunkSize) {
    size_t remaining_data = test_data.size() - i;
    size_t chunk_size = std::min(kDataChunkSize, remaining_data);
    std::vector<uint8_t> chunk(test_data.begin() + i,
                               test_data.begin() + i + chunk_size);
    auto obfuscated_result =
        ObfuscateDataChunk(chunk, derived_key, nonce_prefix, counter++,
                           (i + kDataChunkSize >= test_data.size()));
    ASSERT_TRUE(obfuscated_result.has_value());

    std::move(obfuscated_result.value().begin(),
              obfuscated_result.value().end(),
              std::back_inserter(obfuscated_content));
  }
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

INSTANTIATE_TEST_SUITE_P(
    ObfuscationUtilsFeatureTest,
    ObfuscationUtilsTest,
    ::testing::Combine(
        ::testing::Bool(),  // File obfuscator feature enabled/disabled
        ::testing::Values(0, 10, kDataChunkSize + 1024)));

}  // namespace enterprise_obfuscation
