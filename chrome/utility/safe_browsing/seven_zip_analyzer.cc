// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/safe_browsing/seven_zip_analyzer.h"

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/common/safe_browsing/archive_analyzer_results.h"
#include "components/safe_browsing/content/common/file_type_policies.h"
#include "components/safe_browsing/core/common/features.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
// Must be after <windows.h>
#include <winbase.h>
#elif BUILDFLAG(IS_POSIX)
#include <sys/mman.h>
#endif

namespace safe_browsing {

SevenZipAnalyzer::SevenZipAnalyzer() = default;
SevenZipAnalyzer::~SevenZipAnalyzer() = default;

void SevenZipAnalyzer::OnOpenError(seven_zip::Result result) {
  results()->success = false;
  results()->analysis_result = ArchiveAnalysisResult::kFailedToOpen;
  results()->encryption_info.is_encrypted |=
      result == seven_zip::Result::kEncryptedHeaders;
  if (IsTopLevelArchive()) {
    results()->encryption_info.is_top_level_encrypted |=
        result == seven_zip::Result::kEncryptedHeaders;
  }
}

base::File SevenZipAnalyzer::OnTempFileRequest() {
  return std::move(temp_file2_);
}

bool SevenZipAnalyzer::OnEntry(const seven_zip::EntryInfo& entry,
                               base::span<uint8_t>& output) {
  if (entry.file_size == 0) {
    // Empty files try to initialize the memory mapping with region {0, 0},
    // which is confused with Region::kWholeFile. Since we can't truncate the
    // file within the utility process sandbox, the file still has contents
    // from a previous entry, and we end up mapping those contents. This leads
    // to CHECK failures since `output.size()` does not match
    // `entry.file_size`. Since the file is actually empty, we can skip the
    // memory mapping here to avoid this.
    output = base::span<uint8_t>();
    return true;
  }

  mapped_file_.emplace();
  bool mapped_file_ok = mapped_file_->Initialize(
      temp_file_.Duplicate(), {0, static_cast<size_t>(entry.file_size)},
      base::MemoryMappedFile::READ_WRITE_EXTEND);
  if (!mapped_file_ok) {
    results()->success = false;
    results()->analysis_result = ArchiveAnalysisResult::kUnknown;
    return false;
  }

  output = mapped_file_->mutable_bytes();
  return true;
}

bool SevenZipAnalyzer::OnDirectory(const seven_zip::EntryInfo& entry) {
  return UpdateResultsForEntry(
      temp_file_.Duplicate(), GetRootPath().Append(entry.file_path),
      entry.file_size, entry.is_encrypted, /*is_directory=*/true,
      /*contents_valid=*/!entry.is_encrypted);
}

bool SevenZipAnalyzer::EntryDone(seven_zip::Result result,
                                 const seven_zip::EntryInfo& entry) {
  // Since unpacking an encrypted entry is expected to fail, allow all results
  // here for encrypted entries.
  if (result == seven_zip::Result::kSuccess || entry.is_encrypted) {
    // TODO(crbug.com/40871783): We have the entire file in memory, so it's
    // silly to do all this work to flush it and read it back. Can we simplify
    // this process? This also reduces the risk that the file is not flushed
    // fully.
    mapped_file_.reset();
    if (!UpdateResultsForEntry(
            temp_file_.Duplicate(), GetRootPath().Append(entry.file_path),
            entry.file_size, entry.is_encrypted, /*is_directory=*/false,
            /*contents_valid=*/!entry.is_encrypted)) {
      awaiting_nested_ = true;
      return false;
    }
  }

  return true;
}

void SevenZipAnalyzer::Init() {
  GetTempFile(base::BindOnce(&SevenZipAnalyzer::OnGetTempFile,
                             weak_factory_.GetWeakPtr()));
}

bool SevenZipAnalyzer::ResumeExtraction() {
  awaiting_nested_ = false;
  reader_->Extract();
  return !awaiting_nested_;
}

base::WeakPtr<ArchiveAnalyzer> SevenZipAnalyzer::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void SevenZipAnalyzer::OnGetTempFile(base::File temp_file) {
  if (!temp_file.IsValid()) {
    InitComplete(ArchiveAnalysisResult::kFailedToOpenTempFile);
    return;
  }
  if (!temp_file_.IsValid()) {
    temp_file_ = std::move(temp_file);
    // Get the other temp file, returning here.
    GetTempFile(base::BindOnce(&SevenZipAnalyzer::OnGetTempFile,
                               weak_factory_.GetWeakPtr()));
    return;
  } else {
    temp_file2_ = std::move(temp_file);
  }
  // If the file is too big to unpack, return failure.
  bool too_big_to_unpack =
      base::checked_cast<uint64_t>(GetArchiveFile().GetLength()) >
      FileTypePolicies::GetInstance()->GetMaxFileSizeToAnalyze("7z");
  if (too_big_to_unpack) {
    InitComplete(ArchiveAnalysisResult::kTooLarge);
    return;
  }

  results()->success = true;
  results()->analysis_result = ArchiveAnalysisResult::kValid;

  reader_ =
      seven_zip::SevenZipReader::Create(std::move(GetArchiveFile()), *this);
  if (!reader_) {
    // We will have been notified through OnOpenError and updated `results_`
    // appropriately
    InitComplete(results()->analysis_result);
    return;
  }

  InitComplete(ArchiveAnalysisResult::kValid);
}

}  // namespace safe_browsing
