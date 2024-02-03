// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_test_utils.h"

#include "base/base64.h"
#include "base/ranges/algorithm.h"
#include "components/variations/metrics.h"
#include "components/variations/proto/variations_seed.pb.h"
#include "components/variations/variations_seed_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/compression_utils.h"

namespace variations {

using SignedSeedDataTest = ::testing::TestWithParam<SignedSeedData>;

TEST_P(SignedSeedDataTest, HasValidBase64Data) {
  const auto& signed_seed_data = GetParam();

  std::string decoded_compressed_data;
  ASSERT_TRUE(base::Base64Decode(signed_seed_data.base64_compressed_data,
                                 &decoded_compressed_data));

  std::string actual_uncompressed_data;
  ASSERT_TRUE(compression::GzipUncompress(decoded_compressed_data,
                                          &actual_uncompressed_data));

  std::string actual_encoded_uncompressed_data =
      base::Base64Encode(actual_uncompressed_data);
  EXPECT_EQ(actual_encoded_uncompressed_data,
            signed_seed_data.base64_uncompressed_data);

  std::string decoded_uncompressed_data;
  ASSERT_TRUE(base::Base64Decode(signed_seed_data.base64_uncompressed_data,
                                 &decoded_uncompressed_data));

  EXPECT_EQ(decoded_uncompressed_data, actual_uncompressed_data);
}

TEST_P(SignedSeedDataTest, HasValidSignature) {
  const auto& signed_seed_data = GetParam();
  std::string decoded_uncompressed_data;
  ASSERT_TRUE(base::Base64Decode(signed_seed_data.base64_uncompressed_data,
                                 &decoded_uncompressed_data));

  const auto verify_signature_result =
      VariationsSeedStore::VerifySeedSignatureForTesting(
          decoded_uncompressed_data, signed_seed_data.base64_signature);
  EXPECT_EQ(VerifySignatureResult::VALID_SIGNATURE, verify_signature_result);
}

TEST_P(SignedSeedDataTest, HasStudyNames) {
  const auto& signed_seed_data = GetParam();
  std::string decoded_uncompressed_data;
  ASSERT_TRUE(base::Base64Decode(signed_seed_data.base64_uncompressed_data,
                                 &decoded_uncompressed_data));
  VariationsSeed seed;
  ASSERT_TRUE(seed.ParseFromString(decoded_uncompressed_data));
  std::vector<std::string> parsed_study_names;
  base::ranges::transform(seed.study(), std::back_inserter(parsed_study_names),
                          [](const Study& s) { return s.name(); });
  EXPECT_THAT(parsed_study_names, ::testing::UnorderedElementsAreArray(
                                      signed_seed_data.study_names));
}

INSTANTIATE_TEST_SUITE_P(VariationsTestUtils,
                         SignedSeedDataTest,
                         ::testing::Values(kTestSeedData, kCrashingSeedData));

}  // namespace variations
