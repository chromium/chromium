// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/obfuscation/core/download_obfuscator.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_obfuscation {

namespace {

std::vector<uint8_t> StringToVector(const std::string& str) {
  return std::vector<uint8_t>(str.begin(), str.end());
}

std::string GetHexEncodedHash(const std::unique_ptr<crypto::SecureHash>& hash) {
  std::vector<uint8_t> hash_value(hash->GetHashLength());
  hash->Finish(hash_value.data(), hash_value.size());
  return base::HexEncode(hash_value.data(), hash_value.size());
}

constexpr char kTestData1[] = "Hello, world!";
constexpr char kTestData2[] = "This is another test.";
constexpr char kTestData3[] = "Download obfuscation test.";

constexpr int64_t kOverheadPerChunk = kAuthTagSize + kChunkSizePrefixSize;

struct TestParams {
  std::vector<std::string> chunks;
  bool feature_enabled;
};

}  // namespace

class DownloadObfuscatorTest : public testing::TestWithParam<TestParams> {
 protected:
  void SetUp() override {
    if (GetParam().feature_enabled) {
      feature_list_.InitAndEnableFeature(kEnterpriseFileObfuscation);
    } else {
      feature_list_.InitAndDisableFeature(kEnterpriseFileObfuscation);
    }
  }

  base::test::ScopedFeatureList feature_list_;
};

TEST_P(DownloadObfuscatorTest, ObfuscateAndDeobfuscateVerify) {
  DownloadObfuscator obfuscator;
  const auto& params = GetParam();

  auto expected_hash = crypto::SecureHash::Create(crypto::SecureHash::SHA256);
  size_t expected_overhead = 0;
  std::vector<uint8_t> obfuscated_content;

  // Test obfuscation process.
  for (size_t i = 0; i < params.chunks.size(); ++i) {
    bool is_last_chunk = (i == params.chunks.size() - 1);
    auto result = obfuscator.ObfuscateChunk(StringToVector(params.chunks[i]),
                                            is_last_chunk);

    if (params.feature_enabled) {
      ASSERT_TRUE(result.has_value());
      size_t expected_size = params.chunks[i].size() + kOverheadPerChunk;
      if (i == 0) {
        expected_size += kHeaderSize;
        expected_overhead += kHeaderSize;
      }
      EXPECT_EQ(result->size(), expected_size);
      expected_overhead += kOverheadPerChunk;
      obfuscated_content.insert(obfuscated_content.end(), result->begin(),
                                result->end());
    } else {
      ASSERT_FALSE(result.has_value());
      EXPECT_EQ(result.error(), Error::kDisabled);
      return;
    }

    expected_hash->Update(params.chunks[i].data(), params.chunks[i].size());
  }

  EXPECT_EQ(obfuscator.GetTotalOverhead(),
            static_cast<int64_t>(expected_overhead));

  auto hash = obfuscator.GetUnobfuscatedHash();
  ASSERT_TRUE(hash);
  EXPECT_EQ(GetHexEncodedHash(hash), GetHexEncodedHash(expected_hash));

  // Test deobfuscation process.
  if (params.feature_enabled) {
    DownloadObfuscator deobfuscator;
    size_t offset = 0;
    for (const auto& chunk : params.chunks) {
      auto deobfuscated =
          deobfuscator.DeobfuscateChunk(obfuscated_content, offset);
      ASSERT_TRUE(deobfuscated.has_value());
      EXPECT_EQ(deobfuscated.value(), StringToVector(chunk));
    }
    EXPECT_EQ(offset, obfuscated_content.size());

    // Test overhead calculation using span.
    auto calculated_overhead_span =
        deobfuscator.CalculateDeobfuscationOverhead(obfuscated_content);
    ASSERT_TRUE(calculated_overhead_span.has_value());
    EXPECT_EQ(calculated_overhead_span.value(),
              static_cast<int64_t>(expected_overhead));

    // Test overhead calculation using file.
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
    base::FilePath temp_file_path = temp_dir.GetPath().AppendASCII("temp_file");

    // Write obfuscated content to a temporary file.
    std::string_view obfuscated_data(
        reinterpret_cast<const char*>(obfuscated_content.data()),
        obfuscated_content.size());
    ASSERT_TRUE(base::WriteFile(temp_file_path, obfuscated_data));

    base::File temp_file(temp_file_path,
                         base::File::FLAG_OPEN | base::File::FLAG_READ);
    ASSERT_TRUE(temp_file.IsValid());

    auto calculated_overhead_file =
        deobfuscator.CalculateDeobfuscationOverhead(temp_file);
    ASSERT_TRUE(calculated_overhead_file.has_value());
    EXPECT_EQ(calculated_overhead_file.value(),
              static_cast<int64_t>(expected_overhead));

    EXPECT_EQ(calculated_overhead_span.value(),
              calculated_overhead_file.value());
  }
}

INSTANTIATE_TEST_SUITE_P(
    Variations,
    DownloadObfuscatorTest,
    testing::Values(TestParams{{kTestData1}, true},  // Single chunk
                    TestParams{{kTestData1, kTestData2, kTestData3},
                               true},                // Multiple chunks
                    TestParams{{""}, true},          // Empty input
                    TestParams{{kTestData1}, false}  // Feature disabled
                    ));

class DownloadObfuscatorEnabledTest : public testing::Test {
 protected:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(kEnterpriseFileObfuscation);
  }

  base::test::ScopedFeatureList feature_list_;
};

// Verifies that the obfuscated results should be different for the same input,
// but unobfuscated hash stays the same.
TEST_F(DownloadObfuscatorEnabledTest, ObfuscationConsistency) {
  DownloadObfuscator obfuscator1;
  DownloadObfuscator obfuscator2;

  auto result1 = obfuscator1.ObfuscateChunk(StringToVector(kTestData1), true);
  ASSERT_TRUE(result1.has_value());

  auto result2 = obfuscator2.ObfuscateChunk(StringToVector(kTestData1), true);
  ASSERT_TRUE(result2.has_value());

  EXPECT_NE(*result1, *result2);
  EXPECT_EQ(GetHexEncodedHash(obfuscator1.GetUnobfuscatedHash()),
            GetHexEncodedHash(obfuscator2.GetUnobfuscatedHash()));
}

// Test invalid data scenarios.
TEST_F(DownloadObfuscatorEnabledTest, InvalidData) {
  DownloadObfuscator obfuscator;

  // Test deobfuscation with invalid header.
  std::vector<uint8_t> invalid_header(kHeaderSize - 1, 0);
  size_t offset = 0;
  auto deobfuscate_result = obfuscator.DeobfuscateChunk(invalid_header, offset);
  EXPECT_FALSE(deobfuscate_result.has_value());
  EXPECT_EQ(deobfuscate_result.error(), Error::kDeobfuscationFailed);

  // Test overhead calculation with invalid data.
  std::vector<uint8_t> invalid_data(kHeaderSize + kChunkSizePrefixSize - 1, 0);
  auto overhead_result =
      obfuscator.CalculateDeobfuscationOverhead(invalid_data);
  EXPECT_FALSE(overhead_result.has_value());
  EXPECT_EQ(overhead_result.error(), Error::kDeobfuscationFailed);
}

// Test partial writes for deobfuscation.
TEST_F(DownloadObfuscatorEnabledTest, PartialDeobfuscation) {
  DownloadObfuscator obfuscator;
  std::string test_data = "This is a test for partial deobfuscation.";
  auto obfuscated = obfuscator.ObfuscateChunk(StringToVector(test_data), true);
  ASSERT_TRUE(obfuscated.has_value());

  DownloadObfuscator deobfuscator;
  std::vector<uint8_t> deobfuscated_content;
  size_t total_deobfuscated_size = 0;
  size_t partial_read_size = 5;  // Read in only 5 bytes at a time

  while (total_deobfuscated_size < test_data.size()) {
    // Test that for partial writes, it reads the same obfuscated chunk.
    auto deobfuscated_chunk = deobfuscator.GetNextDeobfuscatedChunk(
        base::make_span(obfuscated.value()));
    ASSERT_TRUE(deobfuscated_chunk.has_value());

    size_t bytes_to_read =
        std::min(partial_read_size, deobfuscated_chunk->size());
    deobfuscated_content.insert(deobfuscated_content.end(),
                                deobfuscated_chunk->begin(),
                                deobfuscated_chunk->begin() + bytes_to_read);

    total_deobfuscated_size += bytes_to_read;
    deobfuscator.UpdateDeobfuscatedChunkPosition(bytes_to_read);
  }

  EXPECT_EQ(deobfuscated_content, StringToVector(test_data));
  EXPECT_EQ(deobfuscator.GetNextChunkOffset(), obfuscated->size());

  // Obfuscate an empty chunk to test scenarios where size is not given.
  auto empty_obfuscated =
      obfuscator.ObfuscateChunk(std::vector<uint8_t>(), true);
  ASSERT_TRUE(empty_obfuscated.has_value());

  auto deobfuscated_chunk = deobfuscator.GetNextDeobfuscatedChunk(
      base::make_span(empty_obfuscated.value()));
  ASSERT_TRUE(deobfuscated_chunk.has_value());
  EXPECT_EQ(deobfuscated_chunk.value(), std::vector<uint8_t>());
}

}  // namespace enterprise_obfuscation
