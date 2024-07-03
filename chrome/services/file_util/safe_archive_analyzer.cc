// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/file_util/safe_archive_analyzer.h"

#include "base/functional/callback.h"
#include "build/build_config.h"
#include "chrome/common/safe_browsing/archive_analyzer_results.h"

namespace {
// The maximum duration of analysis, in milliseconds.
constexpr base::TimeDelta kArchiveAnalysisTimeout = base::Milliseconds(10000);
}  // namespace

SafeArchiveAnalyzer::SafeArchiveAnalyzer() = default;

SafeArchiveAnalyzer::~SafeArchiveAnalyzer() = default;

void SafeArchiveAnalyzer::AnalyzeZipFile(
    base::File zip_file,
    const std::optional<std::string>& password,
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
  zip_analyzer_.Analyze(std::move(zip_file), base::FilePath(), password,
                        std::move(analysis_finished_callback),
                        std::move(temp_file_getter_callback), &results_);
}

void SafeArchiveAnalyzer::AnalyzeDmgFile(
    base::File dmg_file,
    mojo::PendingRemote<chrome::mojom::TemporaryFileGetter> temp_file_getter,
    AnalyzeDmgFileCallback callback) {
#if BUILDFLAG(IS_MAC)
  DCHECK(dmg_file.IsValid());
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
  // TODO(crbug.com/40923881): Update DMG analyzer to use passwords and provide
  // the password here.
  dmg_analyzer_.Analyze(std::move(dmg_file), base::FilePath(),
                        /*password=*/std::nullopt,
                        std::move(analysis_finished_callback),
                        std::move(temp_file_getter_callback), &results_);
#else
  NOTREACHED_IN_MIGRATION();
#endif
}

void SafeArchiveAnalyzer::AnalyzeRarFile(
    base::File rar_file,
    const std::optional<std::string>& password,
    mojo::PendingRemote<chrome::mojom::TemporaryFileGetter> temp_file_getter,
    AnalyzeRarFileCallback callback) {
#if USE_UNRAR
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
  rar_analyzer_.Analyze(std::move(rar_file), base::FilePath(),
                        /*password=*/password,
                        std::move(analysis_finished_callback),
                        std::move(temp_file_getter_callback), &results_);
#else
  std::move(callback).Run(safe_browsing::ArchiveAnalyzerResults());
#endif
}

void SafeArchiveAnalyzer::AnalyzeSevenZipFile(
    base::File seven_zip_file,
    mojo::PendingRemote<chrome::mojom::TemporaryFileGetter> temp_file_getter,
    AnalyzeSevenZipFileCallback callback) {
  DCHECK(seven_zip_file.IsValid());
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
  // TODO(crbug.com/40923881): Update 7Z analyzer to use passwords and provide
  // the password here.
  seven_zip_analyzer_.Analyze(std::move(seven_zip_file), base::FilePath(),
                              /*password=*/std::nullopt,
                              std::move(analysis_finished_callback),
                              std::move(temp_file_getter_callback), &results_);
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
