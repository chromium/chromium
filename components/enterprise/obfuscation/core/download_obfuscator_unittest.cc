// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/obfuscation/core/download_obfuscator.h"

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

// Each obfuscated chunk adds 16 (authentication tag) + 4 (chunk size) = 20
// bytes of overhead. The first chunk also has a 40 byte header prepended.
constexpr int64_t kOverheadPerChunk = 20;
constexpr int64_t kHeaderSize = 40;

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

TEST_P(DownloadObfuscatorTest, ObfuscateAndVerify) {
  DownloadObfuscator obfuscator;
  const auto& params = GetParam();

  auto expected_hash = crypto::SecureHash::Create(crypto::SecureHash::SHA256);
  size_t expected_overhead = 0;

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

// Verifies that the obfuscated results should be different for the same input,
// but unobfuscated hash stays the same.
TEST(DownloadObfuscatorConsistencyTest, ObfuscationConsistency) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kEnterpriseFileObfuscation);

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

}  // namespace enterprise_obfuscation
