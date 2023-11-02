// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/safe_browsing/binary_feature_extractor.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {
namespace {

class BinaryFeatureExtractorMacTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_));
    feature_extractor_ = new BinaryFeatureExtractor();
  }

  base::FilePath GetPath(const char* file_name) {
    return test_data_.AppendASCII("safe_browsing")
                     .AppendASCII("mach_o")
                     .AppendASCII(file_name);
  }

  BinaryFeatureExtractor* feature_extractor() {
    return feature_extractor_.get();
  }

 private:
  base::FilePath test_data_;
  scoped_refptr<BinaryFeatureExtractor> feature_extractor_;
};

TEST_F(BinaryFeatureExtractorMacTest, UnsignedMachOThin) {
  ClientDownloadRequest_ImageHeaders image_headers;
  google::protobuf::RepeatedPtrField<std::string> signed_data;

  base::FilePath path = GetPath("lib32.dylib");
  ASSERT_TRUE(feature_extractor()->ExtractImageFeatures(
                  path, 0, &image_headers, &signed_data));

  EXPECT_EQ(1, image_headers.mach_o_headers().size());
  EXPECT_EQ(0, signed_data.size());
}

TEST_F(BinaryFeatureExtractorMacTest, SignedMachOFat) {
  ClientDownloadRequest_ImageHeaders image_headers;
  google::protobuf::RepeatedPtrField<std::string> signed_data;

  base::FilePath path = GetPath("signedexecutablefat");
  ASSERT_TRUE(feature_extractor()->ExtractImageFeatures(
                  path, 0, &image_headers, &signed_data));

  EXPECT_EQ(2, image_headers.mach_o_headers().size());
  EXPECT_EQ(2, signed_data.size());
}

TEST_F(BinaryFeatureExtractorMacTest, NotMachO) {
  ClientDownloadRequest_ImageHeaders image_headers;
  google::protobuf::RepeatedPtrField<std::string> signed_data;

  base::FilePath path = GetPath("src.c");
  EXPECT_FALSE(feature_extractor()->ExtractImageFeatures(
                   path, 0, &image_headers, &signed_data));

  EXPECT_EQ(0, image_headers.mach_o_headers().size());
  EXPECT_EQ(0, signed_data.size());
}

}  // namespace
}  // namespace safe_browsing
