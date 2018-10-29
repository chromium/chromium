// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SAFE_BROWSING_MOCK_BINARY_FEATURE_EXTRACTOR_H_
#define CHROME_COMMON_SAFE_BROWSING_MOCK_BINARY_FEATURE_EXTRACTOR_H_

#include "chrome/common/safe_browsing/binary_feature_extractor.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace safe_browsing {

class MockBinaryFeatureExtractor : public BinaryFeatureExtractor {
 public:
  MockBinaryFeatureExtractor();
  MOCK_METHOD2(CheckSignature,
               void(const base::FilePath&,
                    ClientDownloadRequest_SignatureInfo*));
  MOCK_METHOD4(ExtractImageFeatures,
               bool(const base::FilePath&,
                    ExtractHeadersOption,
                    ClientDownloadRequest_ImageHeaders*,
                    google::protobuf::RepeatedPtrField<std::string>*));

 protected:
  ~MockBinaryFeatureExtractor() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockBinaryFeatureExtractor);
};

}  // namespace safe_browsing

#endif  // CHROME_COMMON_SAFE_BROWSING_MOCK_BINARY_FEATURE_EXTRACTOR_H_
