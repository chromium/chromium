// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/safe_browsing/archive_analyzer.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/common/safe_browsing/archive_analyzer_results.h"
#include "chrome/utility/safe_browsing/seven_zip_analyzer.h"
#include "chrome/utility/safe_browsing/zip_analyzer.h"
#include "components/safe_browsing/content/common/proto/download_file_types.pb.h"
#include "components/safe_browsing/core/common/features.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/utility/safe_browsing/mac/dmg_analyzer.h"
#endif

#if USE_UNRAR
#include "chrome/utility/safe_browsing/rar_analyzer.h"
#endif

namespace safe_browsing {

// static
std::unique_ptr<ArchiveAnalyzer> ArchiveAnalyzer::CreateForArchiveType(
    DownloadFileType_InspectionType file_type) {
  if (file_type == DownloadFileType::ZIP) {
    return std::make_unique<ZipAnalyzer>();
  } else if (file_type == DownloadFileType::SEVEN_ZIP) {
    return std::make_unique<SevenZipAnalyzer>();
  }

#if BUILDFLAG(IS_MAC)
  if (file_type == DownloadFileType::DMG) {
    return std::make_unique<dmg::DMGAnalyzer>();
  }
#endif

#if USE_UNRAR
  if (file_type == DownloadFileType::RAR) {
    return std::make_unique<RarAnalyzer>();
  }
#endif

  return nullptr;
}

ArchiveAnalyzer::ArchiveAnalyzer() = default;
ArchiveAnalyzer::~ArchiveAnalyzer() = default;

void ArchiveAnalyzer::Analyze(
    base::File archive_file,
    base::FilePath relative_path,
    const std::optional<std::string>& password,
    FinishedAnalysisCallback finished_analysis_callback,
    GetTempFileCallback get_temp_file_callback,
    ArchiveAnalyzerResults* results) {
  archive_file_ = std::move(archive_file);
  root_path_ = relative_path;
  password_ = password;
  finished_analysis_callback_ = std::move(finished_analysis_callback);
  get_temp_file_callback_ = std::move(get_temp_file_callback);
  results_ = results;

  Init();
}

void ArchiveAnalyzer::SetResultsForTesting(ArchiveAnalyzerResults* results) {
  results_ = results;
}

void ArchiveAnalyzer::SetFinishedCallbackForTesting(
    FinishedAnalysisCallback callback) {
  finished_analysis_callback_ = std::move(callback);
}

base::File& ArchiveAnalyzer::GetArchiveFile() {
  return archive_file_;
}
void ArchiveAnalyzer::GetTempFile(
    base::OnceCallback<void(base::File)> callback) {
  get_temp_file_callback_.Run(std::move(callback));
}
const base::FilePath& ArchiveAnalyzer::GetRootPath() const {
  return root_path_;
}

bool ArchiveAnalyzer::UpdateResultsForEntry(base::File entry,
                                            base::FilePath path,
                                            int file_length,
                                            bool is_encrypted,
                                            bool is_directory,
                                            bool contents_valid) {
  if (!is_encrypted) {
    nested_analyzer_ = ArchiveAnalyzer::CreateForArchiveType(GetFileType(path));
    if (nested_analyzer_) {
      // Archive analyzers expect to start at the beginning of the
      // archive, but we may be at the end.
      entry.Seek(base::File::FROM_BEGIN, 0);
      nested_analyzer_->Analyze(
          entry.Duplicate(), path, password(),
          base::BindOnce(&ArchiveAnalyzer::NestedAnalysisFinished, GetWeakPtr(),
                         entry.Duplicate(), path, file_length),
          get_temp_file_callback_, results_);
      return false;
    }
  }

  if (is_directory) {
    results_->directory_count++;
  } else {
    results_->file_count++;
  }

  UpdateArchiveAnalyzerResultsWithFile(path, &entry, file_length, is_encrypted,
                                       is_directory, contents_valid,
                                       IsTopLevelArchive(), results_);
  return true;
}

void ArchiveAnalyzer::InitComplete(ArchiveAnalysisResult result) {
  if (result != ArchiveAnalysisResult::kValid) {
    results_->success = false;
    results_->analysis_result = result;
    std::move(finished_analysis_callback_).Run();
    return;
  }

  results_->success = true;
  results_->analysis_result = ArchiveAnalysisResult::kValid;

  if (ResumeExtraction()) {
    std::move(finished_analysis_callback_).Run();
  }
}

void ArchiveAnalyzer::NestedAnalysisFinished(base::File entry,
                                             base::FilePath path,
                                             int entry_size) {
  // `results_->success` will contain the latest analyzer's success
  // status and can be used to determine if the nester archive unpacked
  // successfully.
  if (!results_->success) {
    results_->has_archive = true;
    results_->file_count++;
    results_->archived_archive_filenames.push_back(path.BaseName());
    ClientDownloadRequest::ArchivedBinary* archived_archive =
        results_->archived_binary.Add();
    archived_archive->set_download_type(ClientDownloadRequest::ARCHIVE);
    archived_archive->set_is_encrypted(false);
    archived_archive->set_is_archive(true);
    SetNameForContainedFile(path, archived_archive);
    SetLengthAndDigestForContainedFile(&entry, entry_size, archived_archive);
  }

  if (ResumeExtraction()) {
    std::move(finished_analysis_callback_).Run();
  }
}

bool ArchiveAnalyzer::IsTopLevelArchive() const {
  return root_path_.empty();
}

}  // namespace safe_browsing
