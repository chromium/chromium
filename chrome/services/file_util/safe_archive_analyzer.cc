// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/file_util/safe_archive_analyzer.h"

#include "base/functional/callback.h"
#include "build/build_config.h"
#include "chrome/common/safe_browsing/archive_analyzer_results.h"
#include "chrome/common/safe_browsing/seven_zip_analyzer.h"
#include "chrome/common/safe_browsing/zip_analyzer.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/utility/safe_browsing/mac/dmg_analyzer.h"
#endif

namespace {
// The maximum duration of analysis, in milliseconds.
constexpr base::TimeDelta kArchiveAnalysisTimeout = base::Milliseconds(10000);
}  // namespace

SafeArchiveAnalyzer::SafeArchiveAnalyzer() = default;

SafeArchiveAnalyzer::~SafeArchiveAnalyzer() = default;

void SafeArchiveAnalyzer::AnalyzeZipFile(
    base::File zip_file,
    mojo::PendingRemote<chrome::mojom::TemporaryFileGetter> temp_file_getter,
    AnalyzeZipFileCallback callback) {
  DCHECK(zip_file.IsValid());
  temp_file_getter_.Bind(std::move(temp_file_getter));
  callback_ = std::move(callback);
  AnalysisFinishedCallback analysis_finished_callback =
      base::BindOnce(&SafeArchiveAnalyzer::AnalysisFinished,
                     weak_factory_.GetWeakPtr(), base::FilePath());

  base::RepeatingCallback<void(GetTempFileCallback callback)>
      temp_file_getter_callback =
          base::BindRepeating(&SafeArchiveAnalyzer::RequestTemporaryFile,
                              weak_factory_.GetWeakPtr());
  timeout_timer_.Start(FROM_HERE, kArchiveAnalysisTimeout, this,
                       &SafeArchiveAnalyzer::Timeout);
  zip_analyzer_.Init(std::move(zip_file), base::FilePath(),
                     std::move(analysis_finished_callback),
                     std::move(temp_file_getter_callback), &results_);
}

void SafeArchiveAnalyzer::AnalyzeDmgFile(base::File dmg_file,
                                         AnalyzeDmgFileCallback callback) {
#if BUILDFLAG(IS_MAC)
  DCHECK(dmg_file.IsValid());
  safe_browsing::ArchiveAnalyzerResults results;
  safe_browsing::dmg::AnalyzeDMGFile(std::move(dmg_file), &results);
  std::move(callback).Run(results);
#else
  NOTREACHED();
#endif
}

void SafeArchiveAnalyzer::AnalyzeRarFile(
    base::File rar_file,
    mojo::PendingRemote<chrome::mojom::TemporaryFileGetter> temp_file_getter,
    AnalyzeRarFileCallback callback) {
  DCHECK(rar_file.IsValid());
  temp_file_getter_.Bind(std::move(temp_file_getter));
  callback_ = std::move(callback);
  AnalysisFinishedCallback analysis_finished_callback =
      base::BindOnce(&SafeArchiveAnalyzer::AnalysisFinished,
                     weak_factory_.GetWeakPtr(), base::FilePath());
  base::RepeatingCallback<void(GetTempFileCallback callback)>
      temp_file_getter_callback =
          base::BindRepeating(&SafeArchiveAnalyzer::RequestTemporaryFile,
                              weak_factory_.GetWeakPtr());
  timeout_timer_.Start(FROM_HERE, kArchiveAnalysisTimeout, this,
                       &SafeArchiveAnalyzer::Timeout);
  rar_analyzer_.Init(std::move(rar_file), base::FilePath(),
                     std::move(analysis_finished_callback),
                     std::move(temp_file_getter_callback), &results_);
}

void SafeArchiveAnalyzer::AnalyzeSevenZipFile(
    base::File seven_zip_file,
    base::File temporary_file,
    base::File temporary_file2,
    AnalyzeSevenZipFileCallback callback) {
  DCHECK(seven_zip_file.IsValid());

  safe_browsing::ArchiveAnalyzerResults results;
  safe_browsing::seven_zip_analyzer::AnalyzeSevenZipFile(
      std::move(seven_zip_file), std::move(temporary_file),
      std::move(temporary_file2), &results);
  std::move(callback).Run(results);
}

void SafeArchiveAnalyzer::RequestTemporaryFile(GetTempFileCallback callback) {
  temp_file_getter_->RequestTemporaryFile(std::move(callback));
}

void SafeArchiveAnalyzer::AnalysisFinished(base::FilePath path) {
  if (callback_) {
    std::move(callback_).Run(results_);
  }
}

void SafeArchiveAnalyzer::Timeout() {
  results_.success = false;
  results_.analysis_result = safe_browsing::ArchiveAnalysisResult::kTimeout;
  if (callback_) {
    std::move(callback_).Run(results_);
  }
}
