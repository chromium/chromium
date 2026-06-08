// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/safe_browsing/mac/dmg_analyzer.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/strings/string_view_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/common/safe_browsing/archive_analyzer_results.h"
#include "chrome/common/safe_browsing/binary_feature_extractor.h"
#include "chrome/common/safe_browsing/mach_o_image_reader_mac.h"
#include "chrome/utility/safe_browsing/mac/dmg_iterator.h"
#include "chrome/utility/safe_browsing/mac/read_stream.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "crypto/hash.h"

namespace safe_browsing {
namespace dmg {

namespace {

// MachOFeatureExtractor examines files to determine if they are Mach-O, and,
// if so, it uses the BinaryFeatureExtractor to obtain information about the
// image. In addition, this class will compute the SHA256 hash of the file.
class MachOFeatureExtractor {
 public:
  explicit MachOFeatureExtractor(std::unique_ptr<ReadStream> stream);

  MachOFeatureExtractor(const MachOFeatureExtractor&) = delete;
  MachOFeatureExtractor& operator=(const MachOFeatureExtractor&) = delete;

  ~MachOFeatureExtractor();

  // Returns whether the stream references a Mach-O image by examining its magic
  // number. If not, the stream is reset and it is an error to attempt further
  // operations.
  bool IsMachO();

  // Computes the hash of the data in the underlying stream and extracts the
  // Mach-O features from the data, populating `result` with the features.
  // Returns true on success, or false on error.
  bool ExtractFeatures(ClientDownloadRequest_ArchivedBinary* result);

 private:
  // Reads the entire stream into `buffer` and updates `digest` with the hash.
  bool HashAndCopyStream(base::span<uint8_t, crypto::hash::kSha256Size> digest,
                         std::vector<uint8_t>& buffer);

  scoped_refptr<BinaryFeatureExtractor> bfe_;
  std::unique_ptr<ReadStream> stream_;
};

MachOFeatureExtractor::MachOFeatureExtractor(std::unique_ptr<ReadStream> stream)
    : bfe_(base::MakeRefCounted<BinaryFeatureExtractor>()),
      stream_(std::move(stream)) {}

MachOFeatureExtractor::~MachOFeatureExtractor() = default;

bool MachOFeatureExtractor::IsMachO() {
  if (!stream_) {
    return false;
  }
  uint32_t magic = 0;
  bool is_mach_o = stream_->ReadType<uint32_t>(magic) &&
                   MachOImageReader::IsMachOMagicValue(magic);
  if (!is_mach_o) {
    stream_.reset();
  }
  return is_mach_o;
}

bool MachOFeatureExtractor::ExtractFeatures(
    ClientDownloadRequest_ArchivedBinary* result) {
  if (!stream_) {
    return false;
  }
  std::array<uint8_t, crypto::hash::kSha256Size> hash;
  std::vector<uint8_t> buffer;
  if (!HashAndCopyStream(hash, buffer)) {
    return false;
  }

  if (!bfe_->ExtractImageFeaturesFromData(
          buffer, 0, result->mutable_image_headers(),
          result->mutable_signature()->mutable_signed_data())) {
    return false;
  }

  result->set_length(buffer.size());
  result->mutable_digests()->set_sha256(base::as_string_view(hash));

  return true;
}

bool MachOFeatureExtractor::HashAndCopyStream(
    base::span<uint8_t, crypto::hash::kSha256Size> hash,
    std::vector<uint8_t>& buffer) {
  if (stream_->Seek(0, SEEK_SET) != 0) {
    return false;
  }

  buffer.clear();
  buffer.reserve(1024 * 1024);
  crypto::hash::Hasher hasher(crypto::hash::HashKind::kSha256);

  size_t bytes_read;
  const size_t kBufferSizeIncrement = 2048;
  do {
    size_t buffer_offset = buffer.size();

    buffer.resize(buffer.size() + kBufferSizeIncrement);
    base::span<uint8_t> read_buf =
        base::span(buffer).last(kBufferSizeIncrement);
    if (!stream_->Read(read_buf, &bytes_read)) {
      return false;
    }

    buffer.resize(buffer_offset + bytes_read);
    read_buf = read_buf.first(bytes_read);
    if (bytes_read) {
      hasher.Update(read_buf);
    }
  } while (bytes_read > 0);

  hasher.Finish(hash);
  return true;
}

// The first few bytes of a DER-encoded pkcs7-signedData object.
constexpr uint8_t kDERPKCS7SignedData[] = {0x30, 0x80, 0x06, 0x09, 0x2a,
                                           0x86, 0x48, 0x86, 0xf7, 0x0d,
                                           0x01, 0x07, 0x02, 0xa0};

}  // namespace

DMGAnalyzer::~DMGAnalyzer() = default;

DMGAnalyzer::DMGAnalyzer() = default;

void DMGAnalyzer::Init() {
  GetTempFile(
      base::BindOnce(&DMGAnalyzer::OnGetTempFile, weak_factory_.GetWeakPtr()));

  read_stream_ =
      std::make_unique<FileReadStream>(GetArchiveFile().GetPlatformFile());
  iterator_ = std::make_unique<DMGIterator>(&*read_stream_);
}

bool DMGAnalyzer::ResumeExtraction() {
  while (iterator_->Next()) {
    std::unique_ptr<ReadStream> stream = iterator_->GetReadStream();
    if (!stream) {
      continue;
    }

    std::string path = base::UTF16ToUTF8(iterator_->GetPath());

    bool is_detached_code_signature_file = base::EndsWith(
        path, "_CodeSignature/CodeSignature", base::CompareCase::SENSITIVE);

    if (is_detached_code_signature_file) {
      results()->has_executable = true;

      auto maybe_signature_contents = ReadEntireStream(*stream);
      if (!maybe_signature_contents.has_value()) {
        continue;
      }
      std::vector<uint8_t>& signature_contents =
          maybe_signature_contents.value();

      if (signature_contents.size() < std::size(kDERPKCS7SignedData)) {
        continue;
      }

      if (base::span(signature_contents)
              .first(std::size(kDERPKCS7SignedData)) != kDERPKCS7SignedData) {
        continue;
      }

      ClientDownloadRequest_DetachedCodeSignature* detached_signature =
          results()->detached_code_signatures.Add();
      detached_signature->set_file_name(path);
      detached_signature->set_contents(signature_contents.data(),
                                       signature_contents.size());
      continue;
    }

    MachOFeatureExtractor feature_extractor(std::move(stream));
    if (feature_extractor.IsMachO()) {
      ClientDownloadRequest_ArchivedBinary* binary =
          results()->archived_binary.Add();
      binary->set_file_path(path);
      if (feature_extractor.ExtractFeatures(binary)) {
        binary->set_download_type(
            ClientDownloadRequest_DownloadType_MAC_EXECUTABLE);
        binary->set_is_executable(true);
        results()->has_executable = true;
      } else {
        results()->archived_binary.RemoveLast();
      }
    } else {
      // Get a new `stream` because it was moved from in previous branches.
      stream = iterator_->GetReadStream();
      DownloadFileType_InspectionType file_type =
          GetFileType(base::FilePath(path));
      if (file_type == DownloadFileType::ZIP ||
          file_type == DownloadFileType::RAR ||
          file_type == DownloadFileType::DMG ||
          file_type == DownloadFileType::SEVEN_ZIP) {
        if (!CopyStreamToFile(*stream, temp_file_)) {
          continue;
        }

        if (!temp_file_.IsValid()) {
          continue;
        }

        // TODO(crbug.com/40871873): Support file length here.
        if (!UpdateResultsForEntry(
                temp_file_.Duplicate(), GetRootPath().Append(path),
                /*file_length=*/0,
                /*is_encrypted=*/false, /*is_directory=*/false,
                /*contents_valid=*/true)) {
          return false;
        }
      }
    }
  }

  return true;
}

void DMGAnalyzer::OnGetTempFile(base::File temp_file) {
  if (!temp_file.IsValid()) {
    InitComplete(ArchiveAnalysisResult::kFailedToOpenTempFile);
    return;
  }

  temp_file_ = std::move(temp_file);

  if (!iterator_->Open()) {
    InitComplete(ArchiveAnalysisResult::kUnknown);
    return;
  } else if (iterator_->IsEmpty()) {
    InitComplete(ArchiveAnalysisResult::kDmgNoPartitions);
    return;
  }

  results()->signature_blob = iterator_->GetCodeSignature();
  InitComplete(ArchiveAnalysisResult::kValid);
}

void DMGAnalyzer::AnalyzeDMGFileForTesting(
    std::unique_ptr<DMGIterator> iterator,
    ArchiveAnalyzerResults* results,
    base::File temp_file,
    FinishedAnalysisCallback callback) {
  SetResultsForTesting(results);                       // IN-TEST
  SetFinishedCallbackForTesting(std::move(callback));  // IN-TEST
  iterator_ = std::move(iterator);
  OnGetTempFile(std::move(temp_file));
}

base::WeakPtr<ArchiveAnalyzer> DMGAnalyzer::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace dmg
}  // namespace safe_browsing
