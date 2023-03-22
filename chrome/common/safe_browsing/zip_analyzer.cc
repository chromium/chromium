// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/safe_browsing/zip_analyzer.h"

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
#include "chrome/common/safe_browsing/rar_analyzer.h"
#include "components/safe_browsing/content/common/file_type_policies.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "third_party/zlib/google/zip_reader.h"

namespace safe_browsing {

ZipAnalyzer::~ZipAnalyzer() = default;

ZipAnalyzer::ZipAnalyzer() = default;

void ZipAnalyzer::Init(base::File zip_file,
                       base::FilePath root_zip_path,
                       FinishedAnalysisCallback finished_analysis_callback,
                       GetTempFileCallback get_temp_file_callback,
                       ArchiveAnalyzerResults* results) {
  results_ = results;
  root_zip_path_ = root_zip_path;
  finished_analysis_callback_ = std::move(finished_analysis_callback);
  get_temp_file_callback_ = get_temp_file_callback;
  zip_file_ = std::move(zip_file);
  get_temp_file_callback_.Run(
      base::BindOnce(&ZipAnalyzer::FilePreChecks, weak_factory_.GetWeakPtr()));
}

void ZipAnalyzer::FilePreChecks(base::File temp_file) {
  if (!temp_file.IsValid()) {
    results_->success = false;
    results_->analysis_result = ArchiveAnalysisResult::kFailedToOpenTempFile;
    std::move(finished_analysis_callback_).Run();
    return;
  }
  if (!reader_.OpenFromPlatformFile(zip_file_.GetPlatformFile())) {
    results_->success = false;
    results_->analysis_result = ArchiveAnalysisResult::kUnknown;
    std::move(finished_analysis_callback_).Run();
    return;
  }

  bool too_big_to_unpack =
      base::checked_cast<uint64_t>(zip_file_.GetLength()) >
      FileTypePolicies::GetInstance()->GetMaxFileSizeToAnalyze("zip");
  if (too_big_to_unpack) {
    results_->success = false;
    results_->analysis_result = ArchiveAnalysisResult::kTooLarge;
    std::move(finished_analysis_callback_).Run();
    return;
  }
  temp_file_ = std::move(temp_file);
  AnalyzeZipFile();
}

void ZipAnalyzer::AnalyzeZipFile() {
  while (const zip::ZipReader::Entry* const entry = reader_.Next()) {
    // Clear the `temp_file` between extractions.
    if (temp_file_.Seek(base::File::Whence::FROM_BEGIN, 0) != 0) {
      PLOG(WARNING) << "Failed seek";
    }

    // Since this code is expected to run within a utility process, this call
    // will fail on some platforms. We handle this by passing the length
    // into `UpdateArchiveAnalyzerResultsWithFile`, which will only consider
    // the appropriate bytes. See crbug.com/1309879 and crbug.com/774762.
    if (!temp_file_.SetLength(0)) {
      PLOG(WARNING) << "Failed truncate";
    }
    zip::FileWriterDelegate writer(&temp_file_);
    reader_.ExtractCurrentEntry(&writer, std::numeric_limits<uint64_t>::max());
    if (entry->is_directory)
      results_->directory_count++;
    else
      results_->file_count++;
    has_encrypted_ |= entry->is_encrypted;
    has_aes_encrypted_ |= entry->uses_aes_encryption;
    if (base::FeatureList::IsEnabled(kNestedArchives) && !entry->is_encrypted &&
        AnalyzeNestedArchive(GetFileType(entry->path), entry->path)) {
      return;
    } else {
      UpdateArchiveAnalyzerResultsWithFile(root_zip_path_.Append(entry->path),
                                           &temp_file_, writer.file_length(),
                                           entry->is_encrypted, results_);
    }
  }

  if (has_encrypted_) {
    base::UmaHistogramBoolean("SBClientDownload.EncryptedZipUsesAes",
                              has_aes_encrypted_);
  }

  if (reader_.ok()) {
    results_->analysis_result = ArchiveAnalysisResult::kValid;
  } else {
    results_->analysis_result = ArchiveAnalysisResult::kFailedDuringIteration;
  }
  results_->success = reader_.ok();
  std::move(finished_analysis_callback_).Run();
}

bool ZipAnalyzer::AnalyzeNestedArchive(
    safe_browsing::DownloadFileType_InspectionType file_type,
    base::FilePath path) {
  // TODO(crbug.com/1373671): Add support for SevenZip, Rar, and Dmg
  // archives.
  FinishedAnalysisCallback nested_analysis_finished_callback =
      base::BindOnce(&ZipAnalyzer::NestedAnalysisFinished,
                     weak_factory_.GetWeakPtr(), root_zip_path_.Append(path));
  if (file_type == DownloadFileType::ZIP) {
    nested_zip_analyzer_ = std::make_unique<safe_browsing::ZipAnalyzer>();
    nested_zip_analyzer_->Init(temp_file_.Duplicate(),
                               root_zip_path_.Append(path),
                               std::move(nested_analysis_finished_callback),
                               get_temp_file_callback_, results_);
    return true;
  } else if (file_type == DownloadFileType::RAR) {
    nested_rar_analyzer_ = std::make_unique<safe_browsing::RarAnalyzer>();
    nested_rar_analyzer_->Init(temp_file_.Duplicate(),
                               root_zip_path_.Append(path),
                               std::move(nested_analysis_finished_callback),
                               get_temp_file_callback_, results_);
    return true;
  }
  return false;
}

void ZipAnalyzer::NestedAnalysisFinished(base::FilePath path) {
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
  AnalyzeZipFile();
}

}  // namespace safe_browsing
