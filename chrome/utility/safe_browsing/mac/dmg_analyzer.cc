// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/safe_browsing/mac/dmg_analyzer.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/common/safe_browsing/archive_analyzer_results.h"
#include "chrome/common/safe_browsing/binary_feature_extractor.h"
#include "chrome/common/safe_browsing/mach_o_image_reader_mac.h"
#include "chrome/common/safe_browsing/rar_analyzer.h"
#include "chrome/common/safe_browsing/zip_analyzer.h"
#include "chrome/utility/safe_browsing/mac/dmg_iterator.h"
#include "chrome/utility/safe_browsing/mac/read_stream.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"

namespace safe_browsing {
namespace dmg {

namespace {

// MachOFeatureExtractor examines files to determine if they are Mach-O, and,
// if so, it uses the BinaryFeatureExtractor to obtain information about the
// image. In addition, this class will compute the SHA256 hash of the file.
class MachOFeatureExtractor {
 public:
  MachOFeatureExtractor();

  MachOFeatureExtractor(const MachOFeatureExtractor&) = delete;
  MachOFeatureExtractor& operator=(const MachOFeatureExtractor&) = delete;

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
    ReadStream* stream,
    uint8_t digest[crypto::kSHA256Length]) {
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

DMGAnalyzer::~DMGAnalyzer() = default;

DMGAnalyzer::DMGAnalyzer() = default;

void DMGAnalyzer::Init(base::File dmg_file,
                       base::FilePath root_dmg_path,
                       FinishedAnalysisCallback finished_analysis_callback,
                       GetTempFileCallback get_temp_file_callback,
                       ArchiveAnalyzerResults* results) {
  results_ = results;
  root_dmg_path_ = root_dmg_path;
  finished_analysis_callback_ = std::move(finished_analysis_callback);
  get_temp_file_callback_ = get_temp_file_callback;
  dmg_file_ = std::move(dmg_file);
  read_stream_ = std::make_unique<FileReadStream>(dmg_file_.GetPlatformFile());
  iterator_ = std::make_unique<DMGIterator>(&*read_stream_);

  get_temp_file_callback_.Run(
      base::BindOnce(&DMGAnalyzer::FilePreChecks, weak_factory_.GetWeakPtr()));
}

void DMGAnalyzer::FilePreChecks(base::File temp_file) {
  bool failed_pre_checks = false;
  if (!temp_file.IsValid()) {
    failed_pre_checks = true;
    results_->analysis_result = ArchiveAnalysisResult::kFailedToOpenTempFile;
  }
  if (!iterator_->Open()) {
    failed_pre_checks = true;
    results_->analysis_result = safe_browsing::ArchiveAnalysisResult::kUnknown;
  } else if (iterator_->IsEmpty()) {
    failed_pre_checks = true;
    results_->analysis_result =
        safe_browsing::ArchiveAnalysisResult::kDmgNoPartitions;
  }

  if (failed_pre_checks) {
    results_->success = false;
    std::move(finished_analysis_callback_).Run();
    return;
  }
  results_->signature_blob = iterator_->GetCodeSignature();
  AnalyzeDMGFile();
}

void DMGAnalyzer::AnalyzeDMGFile() {
  results_->success = false;

  MachOFeatureExtractor feature_extractor;
  while (iterator_->Next()) {
    std::unique_ptr<ReadStream> stream = iterator_->GetReadStream();
    if (!stream) {
      continue;
    }

    std::string path = base::UTF16ToUTF8(iterator_->GetPath());

    bool is_detached_code_signature_file = base::EndsWith(
        path, "_CodeSignature/CodeSignature", base::CompareCase::SENSITIVE);

    if (is_detached_code_signature_file) {
      results_->has_executable = true;

      std::vector<uint8_t> signature_contents;
      if (!ReadEntireStream(stream.get(), &signature_contents)) {
        continue;
      }

      if (signature_contents.size() < std::size(kDERPKCS7SignedData)) {
        continue;
      }

      if (memcmp(kDERPKCS7SignedData, signature_contents.data(),
                 std::size(kDERPKCS7SignedData)) != 0) {
        continue;
      }

      ClientDownloadRequest_DetachedCodeSignature* detached_signature =
          results_->detached_code_signatures.Add();
      detached_signature->set_file_name(path);
      detached_signature->set_contents(signature_contents.data(),
                                       signature_contents.size());
    } else if (base::FeatureList::IsEnabled(kNestedArchives) &&
               AnalyzeNestedArchive(GetFileType(base::FilePath(path)),
                                    base::FilePath(path))) {
      results_->success = false;
      return;
    } else if (feature_extractor.IsMachO(stream.get())) {
      ClientDownloadRequest_ArchivedBinary* binary =
          results_->archived_binary.Add();
      binary->set_file_path(path);

      if (feature_extractor.ExtractFeatures(stream.get(), binary)) {
        binary->set_download_type(
            ClientDownloadRequest_DownloadType_MAC_EXECUTABLE);
        binary->set_is_executable(true);
        results_->has_executable = true;
      } else {
        results_->archived_binary.RemoveLast();
      }
    }
  }

  results_->analysis_result = safe_browsing::ArchiveAnalysisResult::kValid;
  results_->success = true;
  std::move(finished_analysis_callback_).Run();
}

bool DMGAnalyzer::AnalyzeNestedArchive(
    safe_browsing::DownloadFileType_InspectionType file_type,
    base::FilePath path) {
  if (!CopyStreamToFile(iterator_->GetReadStream().get(), temp_file_)) {
    return false;
  }
  // TODO(crbug.com/1373671): Add support for SevenZip archives.
  if (!temp_file_.IsValid()) {
    return false;
  }
  FinishedAnalysisCallback nested_analysis_finished_callback =
      base::BindOnce(&DMGAnalyzer::NestedAnalysisFinished,
                     weak_factory_.GetWeakPtr(), root_dmg_path_.Append(path));
  if (file_type == DownloadFileType::ZIP) {
    nested_zip_analyzer_ = std::make_unique<safe_browsing::ZipAnalyzer>();
    nested_zip_analyzer_->Init(temp_file_.Duplicate(),
                               root_dmg_path_.Append(path),
                               std::move(nested_analysis_finished_callback),
                               get_temp_file_callback_, results_);
    return true;
  } else if (file_type == DownloadFileType::RAR) {
    nested_rar_analyzer_ = std::make_unique<safe_browsing::RarAnalyzer>();
    nested_rar_analyzer_->Init(temp_file_.Duplicate(),
                               root_dmg_path_.Append(path),
                               std::move(nested_analysis_finished_callback),
                               get_temp_file_callback_, results_);
    return true;
  } else if (file_type == DownloadFileType::DMG) {
    nested_dmg_analyzer_ = std::make_unique<DMGAnalyzer>();
    nested_dmg_analyzer_->Init(temp_file_.Duplicate(),
                               root_dmg_path_.Append(path),
                               std::move(nested_analysis_finished_callback),
                               get_temp_file_callback_, results_);
    return true;
  }
  return false;
}

void DMGAnalyzer::NestedAnalysisFinished(base::FilePath path) {
  // `results_->success` will contain the latest analyzer's success
  // status and can be used to determine if the nester archive unpacked
  // successfully.
  if (!results_->success) {
    results_->has_archive = true;
    results_->archived_archive_filenames.push_back(path.BaseName());
    ClientDownloadRequest::ArchivedBinary* archived_archive =
        results_->archived_binary.Add();
    archived_archive->set_download_type(ClientDownloadRequest::ARCHIVE);
    archived_archive->set_is_encrypted(false);
    archived_archive->set_is_archive(true);
    SetNameForContainedFile(path, archived_archive);
    SetLengthAndDigestForContainedFile(&temp_file_, temp_file_.GetLength(),
                                       archived_archive);
  }
  AnalyzeDMGFile();
}

void DMGAnalyzer::AnalyzeDMGFileForTesting(
    std::unique_ptr<DMGIterator> iterator,
    ArchiveAnalyzerResults* results,
    base::File temp_file) {
  results_ = results;
  iterator_ = std::move(iterator);
  finished_analysis_callback_ = base::DoNothing();
  FilePreChecks(std::move(temp_file));
}

}  // namespace dmg
}  // namespace safe_browsing
