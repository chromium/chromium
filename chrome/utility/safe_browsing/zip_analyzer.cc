// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/safe_browsing/zip_analyzer.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <set>

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/checked_math.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/common/safe_browsing/archive_analyzer_results.h"
#include "components/safe_browsing/content/common/file_type_policies.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "third_party/zlib/google/zip_reader.h"

namespace safe_browsing {

namespace {

class WriterDelegate : public zip::FileWriterDelegate {
 public:
  explicit WriterDelegate(base::File* file)
      : zip::FileWriterDelegate(file), has_disk_error_(false) {}
  WriterDelegate(const WriterDelegate&) = delete;
  WriterDelegate& operator=(const WriterDelegate&) = delete;

  ~WriterDelegate() override = default;

  bool PrepareOutput() override {
    bool success = zip::FileWriterDelegate::PrepareOutput();
    has_disk_error_ |= !success;
    return success;
  }
  bool WriteBytes(const char* data, int num_bytes) override {
    bool success = zip::FileWriterDelegate::WriteBytes(data, num_bytes);
    has_disk_error_ |= !success;
    return success;
  }

  bool has_disk_error() const { return has_disk_error_; }

 private:
  bool has_disk_error_;
};

}  // namespace

ZipAnalyzer::ZipAnalyzer() = default;
ZipAnalyzer::~ZipAnalyzer() = default;

void ZipAnalyzer::Init() {
  GetTempFile(
      base::BindOnce(&ZipAnalyzer::OnGetTempFile, weak_factory_.GetWeakPtr()));
}

bool ZipAnalyzer::ResumeExtraction() {
  while (const zip::ZipReader::Entry* const entry = reader_.Next()) {
    // Clear the `temp_file` between extractions.
    if (temp_file_.Seek(base::File::Whence::FROM_BEGIN, 0) != 0) {
      PLOG(WARNING) << "Failed seek";
    }

    // Since this code is expected to run within a utility process, this call
    // will fail on some platforms. We handle this by passing the length
    // into `UpdateResultsForEntry`, which will only consider
    // the appropriate bytes. See crbug.com/1309879 and crbug.com/774762.
    if (!temp_file_.SetLength(0)) {
      PLOG(WARNING) << "Failed truncate";
    }
    WriterDelegate writer(&temp_file_);
    bool extract_success = reader_.ExtractCurrentEntry(
        &writer, std::numeric_limits<uint64_t>::max());

    has_encrypted_ |= entry->is_encrypted;
    has_aes_encrypted_ |= entry->uses_aes_encryption;
    has_disk_error_ |= writer.has_disk_error();

    if (!extract_success && entry->is_encrypted) {
      results()->encryption_info.password_status =
          EncryptionInfo::kKnownIncorrect;
    }

    if (!UpdateResultsForEntry(temp_file_.Duplicate(),
                               GetRootPath().Append(entry->path),
                               writer.file_length(), entry->is_encrypted,
                               entry->is_directory, extract_success)) {
      return false;
    }
  }

  if (has_encrypted_) {
    if (has_aes_encrypted_ && password() && !password()->empty()) {
      results()->encryption_info.password_status = EncryptionInfo::kUnknown;
    } else if (results()->encryption_info.password_status !=
               EncryptionInfo::kKnownIncorrect) {
      results()->encryption_info.password_status =
          EncryptionInfo::kKnownCorrect;
    }
  }

  if (has_disk_error_) {
    results()->analysis_result = ArchiveAnalysisResult::kDiskError;
  } else if (reader_.ok()) {
    results()->analysis_result = ArchiveAnalysisResult::kValid;
  } else {
    results()->analysis_result = ArchiveAnalysisResult::kFailedDuringIteration;
  }

  results()->success = reader_.ok() && !has_disk_error_;
  return true;
}

base::WeakPtr<ArchiveAnalyzer> ZipAnalyzer::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void ZipAnalyzer::OnGetTempFile(base::File temp_file) {
  if (!temp_file.IsValid()) {
    InitComplete(ArchiveAnalysisResult::kFailedToOpenTempFile);
    return;
  }

  if (!reader_.OpenFromPlatformFile(GetArchiveFile().GetPlatformFile())) {
    InitComplete(ArchiveAnalysisResult::kUnknown);
    return;
  }

  bool too_big_to_unpack =
      base::checked_cast<uint64_t>(GetArchiveFile().GetLength()) >
      FileTypePolicies::GetInstance()->GetMaxFileSizeToAnalyze("zip");
  if (too_big_to_unpack) {
    InitComplete(ArchiveAnalysisResult::kTooLarge);
    return;
  }
  temp_file_ = std::move(temp_file);

  if (password().has_value()) {
    reader_.SetPassword(*password());
  }

  InitComplete(ArchiveAnalysisResult::kValid);
}

}  // namespace safe_browsing
