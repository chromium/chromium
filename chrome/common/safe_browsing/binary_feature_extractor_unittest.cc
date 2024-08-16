// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/safe_browsing/binary_feature_extractor.h"

#include <stdint.h>
#include <string.h>

#include <cstdint>
#include <memory>
#include <string_view>

#include "base/base_paths.h"
#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "crypto/sha2.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

using ::testing::_;

namespace {

// A mock BinaryFeatureExtractor that mocks only `ExtractImageFeaturesFromData`
// so that we can test `ExtractImageFeatures`.
class MockBinaryFeatureExtractor : public BinaryFeatureExtractor {
 public:
  MOCK_METHOD(bool,
              ExtractImageFeaturesFromData,
              (base::span<const uint8_t>,
               ExtractHeadersOption,
               ClientDownloadRequest_ImageHeaders*,
               google::protobuf::RepeatedPtrField<std::string>*));

 protected:
  ~MockBinaryFeatureExtractor() override = default;
};

}  // namespace

class BinaryFeatureExtractorTest : public testing::Test {
 protected:
  BinaryFeatureExtractorTest() : extractor_(new BinaryFeatureExtractor()) {}

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    path_ = temp_dir_.GetPath().Append(FILE_PATH_LITERAL("file.dll"));
  }

  // Writes |buffer| to |path_|.
  void WriteFileToHash(base::span<const uint8_t> buffer) {
    base::File file(path_, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    ASSERT_TRUE(file.IsValid());
    std::optional<size_t> result = file.WriteAtCurrentPos(buffer);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(buffer.size(), result.value());
  }

  // Verifies that |path_| hashes to |digest|.
  void ExpectFileDigestEq(const uint8_t* digest) {
    ClientDownloadRequest_Digests digests;
    extractor_->ExtractDigest(path_, &digests);
    EXPECT_TRUE(digests.has_sha256());
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(digest),
                          crypto::kSHA256Length),
              digests.sha256());
  }

  static const int kBlockSize = 1 << 12;
  scoped_refptr<BinaryFeatureExtractor> extractor_;
  base::ScopedTempDir temp_dir_;

  // The path to a file that may be hashed.
  base::FilePath path_;
};

TEST_F(BinaryFeatureExtractorTest, ExtractDigestNoFile) {
  base::FilePath no_file =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("does_not_exist.dll"));

  ClientDownloadRequest_Digests digests;
  extractor_->ExtractDigest(no_file, &digests);
  EXPECT_FALSE(digests.has_sha256());
}

// Hash a file that is less than 1 4k block.
TEST_F(BinaryFeatureExtractorTest, ExtractSmallDigest) {
  constexpr uint8_t kDigest[] = {
      0x70, 0x27, 0x7b, 0xad, 0xfc, 0xb9, 0x97, 0x6b, 0x24, 0xf9, 0x80,
      0x22, 0x26, 0x2c, 0x31, 0xea, 0x8f, 0xb2, 0x1f, 0x54, 0x93, 0x6b,
      0x69, 0x8b, 0x5d, 0x54, 0xd4, 0xd4, 0x21, 0x0b, 0x98, 0xb7};

  constexpr char kFileData[] = "The mountains are robotic.";
  // Trip through `string_view` to avoid grabbing the null terminator.
  WriteFileToHash(base::as_byte_span(std::string_view(kFileData)));
  ExpectFileDigestEq(kDigest);
}

// Hash a file that is exactly 1 4k block.
TEST_F(BinaryFeatureExtractorTest, ExtractOneBlockDigest) {
  constexpr uint8_t kDigest[] = {
      0x4f, 0x93, 0x6e, 0xee, 0x89, 0x55, 0xa5, 0xe7, 0x46, 0xd0, 0x61,
      0x43, 0x54, 0x5f, 0x33, 0x7b, 0xdc, 0x30, 0x3a, 0x4b, 0x18, 0xb4,
      0x82, 0x20, 0xe3, 0x93, 0x4c, 0x65, 0xe0, 0xc1, 0xc0, 0x19};

  const int kDataLen = kBlockSize;
  auto data = base::HeapArray<uint8_t>::Uninit(kDataLen);
  base::ranges::fill(data, 71);
  WriteFileToHash(data);
  ExpectFileDigestEq(kDigest);
}

// Hash a file that is larger than 1 4k block.
TEST_F(BinaryFeatureExtractorTest, ExtractBigBlockDigest) {
  constexpr uint8_t kDigest[] = {
      0xda, 0xae, 0xa0, 0xd5, 0x3b, 0xce, 0x0b, 0x4e, 0x5f, 0x5d, 0x0b,
      0xc7, 0x6a, 0x69, 0x0e, 0xf1, 0x8b, 0x2d, 0x20, 0xcd, 0xf2, 0x6d,
      0x33, 0xa7, 0x70, 0xf3, 0x6b, 0x85, 0xbf, 0xce, 0x9d, 0x5c};

  const int kDataLen = kBlockSize + 1;
  auto data = base::HeapArray<uint8_t>::Uninit(kDataLen);
  base::ranges::fill(data, 71);
  WriteFileToHash(data);
  ExpectFileDigestEq(kDigest);
}

TEST_F(BinaryFeatureExtractorTest, CanRemoveFileDuringExecution) {
  // mmap fails if the length parameter is 0, so we need a non-empty file.
  char data = ' ';
  WriteFileToHash(base::byte_span_from_ref(data));

  scoped_refptr<MockBinaryFeatureExtractor> mock_extractor(
      new MockBinaryFeatureExtractor());
  EXPECT_CALL(*mock_extractor, ExtractImageFeaturesFromData(_, _, _, _))
      .WillOnce(
          [&](base::span<const uint8_t> data,
              BinaryFeatureExtractor::ExtractHeadersOption options,
              ClientDownloadRequest_ImageHeaders* image_headers,
              google::protobuf::RepeatedPtrField<std::string>* signed_data) {
            EXPECT_TRUE(base::DeleteFile(path_));
            return true;
          });

  ClientDownloadRequest_ImageHeaders image_headers;
  mock_extractor->ExtractImageFeatures(
      path_, BinaryFeatureExtractor::kDefaultOptions, &image_headers, nullptr);
}

}  // namespace safe_browsing
