// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/safe_browsing/rar_analyzer.h"

#include <memory>
#include <string>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/i18n/streaming_utf8_validator.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/checked_math.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/common/safe_browsing/archive_analyzer_results.h"
#include "chrome/common/safe_browsing/download_type_util.h"
#include "chrome/common/safe_browsing/zip_analyzer.h"
#include "components/safe_browsing/content/common/file_type_policies.h"
#include "components/safe_browsing/core/common/features.h"
#include "third_party/unrar/google/unrar_wrapper.h"

namespace safe_browsing {

RarAnalyzer::~RarAnalyzer() = default;

RarAnalyzer::RarAnalyzer() = default;

void RarAnalyzer::Init(base::File rar_file,
                       base::FilePath root_rar_path,
                       FinishedAnalysisCallback finished_analysis_callback,
                       GetTempFileCallback get_temp_file_callback,
                       ArchiveAnalyzerResults* results) {
  results_ = results;
  root_rar_path_ = root_rar_path;
  finished_analysis_callback_ = std::move(finished_analysis_callback);
  get_temp_file_callback_ = get_temp_file_callback;
  rar_file_ = std::move(rar_file);
  get_temp_file_callback_.Run(
      base::BindOnce(&RarAnalyzer::FilePreChecks, weak_factory_.GetWeakPtr()));
}

void RarAnalyzer::FilePreChecks(base::File temp_file) {
  if (!temp_file.IsValid()) {
    results_->success = false;
    results_->analysis_result = ArchiveAnalysisResult::kFailedToOpenTempFile;
    std::move(finished_analysis_callback_).Run();
    return;
  }
  temp_file_ = std::move(temp_file);
  // If the file is too big to unpack, return failure. This will still send a
  // ping as an "invalid" RAR.
  bool too_big_to_unpack =
      base::checked_cast<uint64_t>(rar_file_.GetLength()) >
      FileTypePolicies::GetInstance()->GetMaxFileSizeToAnalyze("rar");
  if (too_big_to_unpack) {
    results_->success = false;
    results_->analysis_result = ArchiveAnalysisResult::kTooLarge;
    std::move(finished_analysis_callback_).Run();
    return;
  }
  // `rar_file_` is consumed by the reader and cannot be used after
  // this point.
  if (!reader_.Open(std::move(rar_file_), temp_file_.Duplicate())) {
    results_->success = false;
    results_->analysis_result = ArchiveAnalysisResult::kUnknown;
    std::move(finished_analysis_callback_).Run();
    return;
  }
  AnalyzeRarFile();
}

void RarAnalyzer::AnalyzeRarFile() {
  results_->success = false;
  while (reader_.ExtractNextEntry()) {
    const third_party_unrar::RarReader::EntryInfo& entry =
        reader_.current_entry();
    if (entry.is_directory) {
      results_->directory_count++;
    } else {
      results_->file_count++;
    }
    has_encrypted_ |= entry.is_encrypted;
    if (base::FeatureList::IsEnabled(kNestedArchives) && !entry.is_encrypted &&
        AnalyzeNestedArchive(GetFileType(entry.file_path), entry.file_path)) {
      return;
    } else {
      UpdateArchiveAnalyzerResultsWithFile(
          root_rar_path_.Append(entry.file_path), &temp_file_, entry.file_size,
          entry.is_encrypted, results_);
    }
  }
  results_->success = true;
  results_->analysis_result = ArchiveAnalysisResult::kValid;
  std::move(finished_analysis_callback_).Run();
}

bool RarAnalyzer::AnalyzeNestedArchive(
    safe_browsing::DownloadFileType_InspectionType file_type,
    base::FilePath path) {
  FinishedAnalysisCallback nested_analysis_finished_callback =
      base::BindOnce(&RarAnalyzer::NestedAnalysisFinished,
                     weak_factory_.GetWeakPtr(), root_rar_path_.Append(path));
  if (file_type == DownloadFileType::ZIP) {
    nested_zip_analyzer_ = std::make_unique<safe_browsing::ZipAnalyzer>();
    nested_zip_analyzer_->Init(temp_file_.Duplicate(),
                               root_rar_path_.Append(path),
                               std::move(nested_analysis_finished_callback),
                               get_temp_file_callback_, results_);
    return true;
  } else if (file_type == DownloadFileType::RAR) {
    nested_rar_analyzer_ = std::make_unique<safe_browsing::RarAnalyzer>();
    nested_rar_analyzer_->Init(temp_file_.Duplicate(),
                               root_rar_path_.Append(path),
                               std::move(nested_analysis_finished_callback),
                               get_temp_file_callback_, results_);
    return true;
  }
  return false;
}

void RarAnalyzer::NestedAnalysisFinished(base::FilePath path) {
  // `results_->success` will contain the latest analyzer's success
  // status and can be used to determine if the nester archive unpacked
  // successfully.
  // TODO(crbug.com/1373671): Add support for SevenZip, Rar, and Dmg
  // archives.
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
  AnalyzeRarFile();
}

}  // namespace safe_browsing
