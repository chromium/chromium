// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/safe_browsing/mac/dmg_analyzer.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/common/safe_browsing/archive_analyzer_results.h"
#include "chrome/common/safe_browsing/binary_feature_extractor.h"
#include "chrome/common/safe_browsing/mach_o_image_reader_mac.h"
#include "chrome/utility/safe_browsing/mac/dmg_iterator.h"
#include "chrome/utility/safe_browsing/mac/read_stream.h"
#include "components/safe_browsing/proto/csd.pb.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"

namespace safe_browsing {
namespace dmg {

namespace {

// The maximum duration of DMG analysis, in milliseconds.
const double kDmgAnalysisTimeoutMs = 10000;

// MachOFeatureExtractor examines files to determine if they are Mach-O, and,
// if so, it uses the BinaryFeatureExtractor to obtain information about the
// image. In addition, this class will compute the SHA256 hash of the file.
class MachOFeatureExtractor {
 public:
  MachOFeatureExtractor();
  ~MachOFeatureExtractor();

  // Tests if the stream references a Mach-O image by examinig its magic
  // number.
  bool IsMachO(ReadStream* stream);

  // Computes the hash of the data in |stream| and extracts the Mach-O
  // features from the data. Returns true if successful, or false on error or
  // if the file was not Mach-O.
  bool ExtractFeatures(ReadStream* stream,
                       ClientDownloadRequest_ArchivedBinary* result);

 private:
  // Reads the entire stream and updates the hash.
  bool HashAndCopyStream(ReadStream* stream,
                         uint8_t digest[crypto::kSHA256Length]);

  scoped_refptr<BinaryFeatureExtractor> bfe_;
  std::vector<uint8_t> buffer_;  // Buffer that contains read stream data.

  DISALLOW_COPY_AND_ASSIGN(MachOFeatureExtractor);
};

MachOFeatureExtractor::MachOFeatureExtractor()
    : bfe_(new BinaryFeatureExtractor()),
      buffer_() {
  buffer_.reserve(1024 * 1024);
}

MachOFeatureExtractor::~MachOFeatureExtractor() {}

bool MachOFeatureExtractor::IsMachO(ReadStream* stream) {
  uint32_t magic = 0;
  return stream->ReadType<uint32_t>(&magic) &&
         MachOImageReader::IsMachOMagicValue(magic);
}

bool MachOFeatureExtractor::ExtractFeatures(
    ReadStream* stream,
    ClientDownloadRequest_ArchivedBinary* result) {
  uint8_t digest[crypto::kSHA256Length];
  if (!HashAndCopyStream(stream, digest))
    return false;

  if (!bfe_->ExtractImageFeaturesFromData(
          &buffer_[0], buffer_.size(), 0,
          result->mutable_image_headers(),
          result->mutable_signature()->mutable_signed_data())) {
    return false;
  }

  result->set_length(buffer_.size());
  result->mutable_digests()->set_sha256(digest, sizeof(digest));

  return true;
}

bool MachOFeatureExtractor::HashAndCopyStream(
    ReadStream* stream, uint8_t digest[crypto::kSHA256Length]) {
  if (stream->Seek(0, SEEK_SET) != 0)
    return false;

  buffer_.clear();
  std::unique_ptr<crypto::SecureHash> sha256(
      crypto::SecureHash::Create(crypto::SecureHash::SHA256));

  size_t bytes_read;
  const size_t kBufferSize = 2048;
  do {
    size_t buffer_offset = buffer_.size();

    buffer_.resize(buffer_.size() + kBufferSize);
    if (!stream->Read(&buffer_[buffer_offset], kBufferSize, &bytes_read))
      return false;

    buffer_.resize(buffer_offset + bytes_read);
    if (bytes_read)
      sha256->Update(&buffer_[buffer_offset], bytes_read);
  } while (bytes_read > 0);

  sha256->Finish(digest, crypto::kSHA256Length);

  return true;
}

// The first few bytes of a DER-encoded pkcs7-signedData object.
constexpr uint8_t kDERPKCS7SignedData[] = {0x30, 0x80, 0x06, 0x09, 0x2a,
                                           0x86, 0x48, 0x86, 0xf7, 0x0d,
                                           0x01, 0x07, 0x02, 0xa0};

}  // namespace

void AnalyzeDMGFile(base::File dmg_file, ArchiveAnalyzerResults* results) {
  FileReadStream read_stream(dmg_file.GetPlatformFile());
  DMGIterator iterator(&read_stream);
  AnalyzeDMGFile(&iterator, results);
}

void AnalyzeDMGFile(DMGIterator* iterator, ArchiveAnalyzerResults* results) {
  base::Time start_time = base::Time::Now();
  results->success = false;

  if (!iterator->Open())
    return;

  MachOFeatureExtractor feature_extractor;

  results->signature_blob = iterator->GetCodeSignature();

  bool timeout = false;
  while (iterator->Next()) {
    std::unique_ptr<ReadStream> stream = iterator->GetReadStream();
    if (!stream)
      continue;
    if (base::Time::Now() - start_time >=
        base::TimeDelta::FromMilliseconds(kDmgAnalysisTimeoutMs)) {
      timeout = true;
      break;
    }

    std::string path = base::UTF16ToUTF8(iterator->GetPath());

    bool is_detached_code_signature_file = base::EndsWith(
        path, "_CodeSignature/CodeSignature", base::CompareCase::SENSITIVE);

    if (is_detached_code_signature_file) {
      results->has_executable = true;

      std::vector<uint8_t> signature_contents;
      if (!ReadEntireStream(stream.get(), &signature_contents))
        continue;

      if (signature_contents.size() < base::size(kDERPKCS7SignedData))
        continue;

      if (memcmp(kDERPKCS7SignedData, signature_contents.data(),
                 base::size(kDERPKCS7SignedData)) != 0) {
        continue;
      }

      ClientDownloadRequest_DetachedCodeSignature* detached_signature =
          results->detached_code_signatures.Add();
      detached_signature->set_file_name(path);
      detached_signature->set_contents(signature_contents.data(),
                                       signature_contents.size());
    } else if (feature_extractor.IsMachO(stream.get())) {
      ClientDownloadRequest_ArchivedBinary* binary =
          results->archived_binary.Add();
      binary->set_file_basename(path);

      if (feature_extractor.ExtractFeatures(stream.get(), binary)) {
        binary->set_download_type(
            ClientDownloadRequest_DownloadType_MAC_EXECUTABLE);
        results->has_executable = true;
      } else {
        results->archived_binary.RemoveLast();
      }
    }
  }

  if (!timeout)
    results->success = true;
}

}  // namespace dmg
}  // namespace safe_browsing
