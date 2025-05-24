// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/safe_browsing/binary_feature_extractor.h"

#include <memory>
#include <utility>

#include "base/containers/heap_array.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/metrics/histogram_functions.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "crypto/hash.h"

namespace safe_browsing {

BinaryFeatureExtractor::BinaryFeatureExtractor() = default;

BinaryFeatureExtractor::~BinaryFeatureExtractor() = default;

bool BinaryFeatureExtractor::ExtractImageFeatures(
    const base::FilePath& file_path,
    ExtractHeadersOption options,
    ClientDownloadRequest_ImageHeaders* image_headers,
    google::protobuf::RepeatedPtrField<std::string>* signed_data) {
  base::FilePath temp_path;
  if (!base::CreateTemporaryFile(&temp_path)) {
    return false;
  }

  if (!base::CopyFile(file_path, temp_path)) {
    base::DeleteFile(temp_path);
    return false;
  }

  base::File temp_file;
  temp_file.Initialize(temp_path, base::File::FLAG_OPEN |
                                      base::File::FLAG_READ |
                                      base::File::FLAG_WIN_TEMPORARY |
                                      base::File::FLAG_DELETE_ON_CLOSE);

  base::MemoryMappedFile mapped_file;
  if (!mapped_file.Initialize(std::move(temp_file))) {
    return false;
  }
  return ExtractImageFeaturesFromData(mapped_file.bytes(), options,
                                      image_headers, signed_data);
}

bool BinaryFeatureExtractor::ExtractImageFeaturesFromFile(
    base::File file,
    ExtractHeadersOption options,
    ClientDownloadRequest_ImageHeaders* image_headers,
    google::protobuf::RepeatedPtrField<std::string>* signed_data) {
  base::MemoryMappedFile mapped_file;
  if (!mapped_file.Initialize(std::move(file)))
    return false;
  return ExtractImageFeaturesFromData(mapped_file.bytes(), options,
                                      image_headers, signed_data);
}

void BinaryFeatureExtractor::ExtractDigest(
    const base::FilePath& file_path,
    ClientDownloadRequest_Digests* digests) {
  base::File file(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (file.IsValid()) {
    auto buf = base::HeapArray<uint8_t>::Uninit(1 << 12);
    crypto::hash::Hasher hasher(crypto::hash::HashKind::kSha256);
    std::optional<size_t> result;
    while (true) {
      result = file.ReadAtCurrentPos(buf);
      if (!result.has_value() || result.value() == 0) {
        break;
      }
      hasher.Update(buf.first(*result));
    }
    // The loop was broken out of because of EOF, not an error.
    if (result.has_value() && result.value() == 0) {
      std::array<uint8_t, crypto::hash::kSha256Size> hash;
      hasher.Finish(hash);
      digests->set_sha256(base::as_string_view(hash));
    }
  }
}

}  // namespace safe_browsing
