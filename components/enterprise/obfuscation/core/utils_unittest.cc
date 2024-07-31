// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/obfuscation/core/utils.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/rand_util.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_obfuscation {
namespace {

constexpr size_t kDataChunkSize = 524288;  // default download buffer size
constexpr size_t kNonceSize = 12u;
constexpr size_t kAuthTagSize = 16u;

// Helper function to generate a random string of the given size.
std::string GetRandomLargeData(size_t size) {
  std::string data(size, '\0');
  for (char& c : data) {
    c = static_cast<char>(base::RandInt(0, 255));
  }
  return data;
}

}  // namespace

class ObfuscationUtilsTest
    : public ::testing::TestWithParam<std::tuple<bool, std::string>> {
 protected:
  ObfuscationUtilsTest() {
    feature_list_.InitWithFeatureState(kEnterpriseFileObfuscation,
                                       file_obfuscation_feature_enabled());
  }

  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  base::FilePath test_file_path() const {
    return temp_dir_.GetPath().AppendASCII("test_file.txt");
  }

  std::string test_data() const { return std::get<1>(GetParam()); }

  bool file_obfuscation_feature_enabled() { return std::get<0>(GetParam()); }

  base::test::ScopedFeatureList feature_list_;
  base::ScopedTempDir temp_dir_;
};

TEST_P(ObfuscationUtilsTest, ObfuscateAndDeobfuscateDataChunk) {
  // Obfuscate the data chunk.
  auto obfuscated_result = ObfuscateDataChunk(base::as_byte_span(test_data()));

  if (!file_obfuscation_feature_enabled()) {
    ASSERT_EQ(obfuscated_result.error(), Error::kDisabled);
    return;
  }

  ASSERT_TRUE(obfuscated_result.has_value());
  ASSERT_NE(obfuscated_result.value(), base::as_byte_span(test_data()));

  // Deobfuscate the data chunk.
  auto deobfuscated_result = DeobfuscateDataChunk(obfuscated_result.value());
  ASSERT_TRUE(deobfuscated_result.has_value());
  EXPECT_EQ(deobfuscated_result.value(), base::as_byte_span(test_data()));

  // Deobfuscation should fail when we modify the ciphertext.
  obfuscated_result.value()[0] ^= 1;
  deobfuscated_result = DeobfuscateDataChunk(obfuscated_result.value());
  ASSERT_EQ(deobfuscated_result.error(), Error::kDeobfuscationFailed);
}

TEST_P(ObfuscationUtilsTest, DeobfuscateFileInPlace) {
  ASSERT_TRUE(base::WriteFile(test_file_path(), test_data()));

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
  size_t num_chunks =
      (test_data().length() + kDataChunkSize - 1) / kDataChunkSize;
  obfuscated_content.reserve(test_data().size() +
                             num_chunks * (kNonceSize + kAuthTagSize));

  // Obufscate in chunks of kChunkSize if the content is large
  if (test_data().size() > kDataChunkSize) {
    for (size_t i = 0; i < test_data().size(); i += kDataChunkSize) {
      std::string chunk = test_data().substr(i, kDataChunkSize);

      auto obfuscated_result = ObfuscateDataChunk(base::as_byte_span(chunk));
      ASSERT_TRUE(obfuscated_result.has_value());

      std::move(obfuscated_result.value().begin(),
                obfuscated_result.value().end(),
                std::back_inserter(obfuscated_content));
    }
  } else {  // For smaller content, obfuscate in one go
    auto obfuscated_result =
        ObfuscateDataChunk(base::as_byte_span(test_data()));
    ASSERT_TRUE(obfuscated_result.has_value());
    obfuscated_content = std::move(obfuscated_result.value());
  }
  ASSERT_TRUE(base::WriteFile(test_file_path(), obfuscated_content));

  ASSERT_TRUE(DeobfuscateFileInPlace(test_file_path()).has_value());

  std::string deobfuscated_content;
  ASSERT_TRUE(base::ReadFileToString(test_file_path(), &deobfuscated_content));
  EXPECT_EQ(deobfuscated_content, test_data());

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
        ::testing::Values("",
                          "Small data",
                          GetRandomLargeData(kDataChunkSize + 1024))));

}  // namespace enterprise_obfuscation
