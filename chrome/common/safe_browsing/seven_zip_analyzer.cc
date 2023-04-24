// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/safe_browsing/seven_zip_analyzer.h"

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
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
#include <sys/mman.h>
#endif

namespace safe_browsing {

SevenZipAnalyzer::~SevenZipAnalyzer() = default;

SevenZipAnalyzer::SevenZipAnalyzer() = default;

void SevenZipAnalyzer::Init(base::File seven_zip_file,
                            base::FilePath seven_zip_path,
                            FinishedAnalysisCallback finished_analysis_callback,
                            GetTempFileCallback get_temp_file_callback,
                            ArchiveAnalyzerResults* results) {
  results_ = results;
  root_seven_zip_path_ = seven_zip_path;
  finished_analysis_callback_ = std::move(finished_analysis_callback);
  get_temp_file_callback_ = get_temp_file_callback;
  seven_zip_file_ = std::move(seven_zip_file);
  get_temp_file_callback_.Run(base::BindOnce(&SevenZipAnalyzer::FilePreChecks,
                                             weak_factory_.GetWeakPtr()));
  get_temp_file_callback_.Run(base::BindOnce(&SevenZipAnalyzer::FilePreChecks,
                                             weak_factory_.GetWeakPtr()));
}

void SevenZipAnalyzer::FilePreChecks(base::File temp_file) {
  if (!temp_file.IsValid()) {
    results_->success = false;
    results_->analysis_result = ArchiveAnalysisResult::kFailedToOpenTempFile;
    std::move(finished_analysis_callback_).Run();
    return;
  }
  if (!temp_file_.IsValid()) {
    temp_file_ = std::move(temp_file);
    return;
  } else {
    temp_file2_ = std::move(temp_file);
  }
  // If the file is too big to unpack, return failure.
  bool too_big_to_unpack =
      base::checked_cast<uint64_t>(seven_zip_file_.GetLength()) >
      FileTypePolicies::GetInstance()->GetMaxFileSizeToAnalyze("7z");
  if (too_big_to_unpack) {
    results_->success = false;
    results_->analysis_result = ArchiveAnalysisResult::kTooLarge;
    std::move(finished_analysis_callback_).Run();
    return;
  }

  results_->success = true;
  results_->analysis_result = ArchiveAnalysisResult::kValid;

  reader_ =
      seven_zip::SevenZipReader::Create(std::move(seven_zip_file_), *this);
  if (!reader_) {
    // We will have been notified through OnOpenError and updated `results_`
    // appropriately
    std::move(finished_analysis_callback_).Run();
    return;
  }

  AnalyzeSevenZipFile();
}

void SevenZipAnalyzer::AnalyzeSevenZipFile() {
  reader_->Extract();

  if (!awaiting_nested_) {
    std::move(finished_analysis_callback_).Run();
  }
}

void SevenZipAnalyzer::OnOpenError(seven_zip::Result result) {
  results_->success = false;
  results_->analysis_result = ArchiveAnalysisResult::kFailedToOpen;
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
    results_->success = false;
    results_->analysis_result = ArchiveAnalysisResult::kUnknown;
    return false;
  }

  output = base::span<uint8_t>(mapped_file_->data(), mapped_file_->length());
  return true;
}

bool SevenZipAnalyzer::OnDirectory(const seven_zip::EntryInfo& entry) {
  UpdateArchiveAnalyzerResultsWithFile(entry.file_path, &temp_file_,
                                       entry.file_size, entry.is_encrypted,
                                       /*is_directory=*/true, results_);

  results_->directory_count++;
  return true;
}

bool SevenZipAnalyzer::EntryDone(seven_zip::Result result,
                                 const seven_zip::EntryInfo& entry) {
  base::UmaHistogramEnumeration("SBClientDownload.SevenZipEntryResult", result);

  results_->file_count++;

  // Since unpacking an encrypted entry is expected to fail, allow all results
  // here for encrypted entries.
  if (result == seven_zip::Result::kSuccess || entry.is_encrypted) {
    if (base::FeatureList::IsEnabled(kNestedArchives) && !entry.is_encrypted &&
        AnalyzeNestedArchive(GetFileType(entry.file_path), entry.file_path)) {
      awaiting_nested_ = true;
      return false;
    } else {
      // TODO(crbug/1373509): We have the entire file in memory, so it's silly
      // to do all this work to flush it and read it back. Can we simplify this
      // process? This also reduces the risk that the file is not flushed fully.
      mapped_file_.reset();
      UpdateArchiveAnalyzerResultsWithFile(
          root_seven_zip_path_.Append(entry.file_path), &temp_file_,
          entry.file_size, entry.is_encrypted,
          /*is_directory=*/false, results_);
    }
  } else {
    results_->success = false;
  }

  return true;
}

bool SevenZipAnalyzer::AnalyzeNestedArchive(
    safe_browsing::DownloadFileType_InspectionType file_type,
    base::FilePath path) {
  FinishedAnalysisCallback nested_analysis_finished_callback = base::BindOnce(
      &SevenZipAnalyzer::NestedAnalysisFinished, weak_factory_.GetWeakPtr(),
      root_seven_zip_path_.Append(path));
  if (file_type == DownloadFileType::ZIP) {
    nested_zip_analyzer_ = std::make_unique<safe_browsing::ZipAnalyzer>();
    nested_zip_analyzer_->Init(temp_file_.Duplicate(),
                               root_seven_zip_path_.Append(path),
                               std::move(nested_analysis_finished_callback),
                               get_temp_file_callback_, results_);
    return true;
  } else if (file_type == DownloadFileType::RAR) {
    nested_rar_analyzer_ = std::make_unique<safe_browsing::RarAnalyzer>();
    nested_rar_analyzer_->Init(temp_file_.Duplicate(),
                               root_seven_zip_path_.Append(path),
                               std::move(nested_analysis_finished_callback),
                               get_temp_file_callback_, results_);
    return true;
  } else if (file_type == DownloadFileType::SEVEN_ZIP) {
    nested_seven_zip_analyzer_ =
        std::make_unique<safe_browsing::SevenZipAnalyzer>();
    nested_seven_zip_analyzer_->Init(
        temp_file_.Duplicate(), root_seven_zip_path_.Append(path),
        std::move(nested_analysis_finished_callback), get_temp_file_callback_,
        results_);
    return true;
  }

  return false;
}

void SevenZipAnalyzer::NestedAnalysisFinished(base::FilePath path) {
  awaiting_nested_ = false;

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

  AnalyzeSevenZipFile();
}

}  // namespace safe_browsing
