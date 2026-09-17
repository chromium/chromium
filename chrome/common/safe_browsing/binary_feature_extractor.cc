// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/safe_browsing/binary_feature_extractor.h"

#include "build/build_config.h"

#include <memory>
#include <utility>

#include "base/containers/heap_array.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_view_util.h"
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
  base::FilePath temp_dir;
  if (!base::GetTempDir(&temp_dir)) {
    return false;
  }

  base::FilePath temp_path;
  base::File temp_file = base::CreateAndOpenTemporaryFileInDir(
      temp_dir, &temp_path,
      base::File::FLAG_WIN_TEMPORARY | base::File::FLAG_DELETE_ON_CLOSE);
  if (!temp_file.IsValid()) {
    return false;
  }

#if !BUILDFLAG(IS_WIN)
  // CreateAndOpenTemporaryFileInDir delegates deletion to the caller on POSIX.
  // We unlink the file immediately after creation to ensure it is deleted even
  // if the process crashes, while keeping the file descriptor open for use.
  base::DeleteFile(temp_path);
#endif

  {
    // FLAG_WIN_SHARE_DELETE allows the download's rename to its final name to
    // proceed on Windows while this read handle is still open, since the
    // rename can otherwise take several retries to succeed on large files
    // (https://crbug.com/545877431).
    base::File source_file(file_path, base::File::FLAG_OPEN |
                                          base::File::FLAG_READ |
                                          base::File::FLAG_WIN_SHARE_DELETE);
    if (!source_file.IsValid()) {
      return false;
    }

    if (!base::CopyFileContents(source_file, temp_file)) {
      base::DeleteFile(temp_path);
      return false;
    }
  }

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
  // See the comment in ExtractImageFeatures() about FLAG_WIN_SHARE_DELETE.
  base::File file(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                                 base::File::FLAG_WIN_SHARE_DELETE);
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
