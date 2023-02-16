// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/file_util/public/cpp/sandboxed_seven_zip_analyzer.h"

#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/thread_pool.h"
#include "chrome/common/safe_browsing/archive_analyzer_results.h"
#include "chrome/services/file_util/public/mojom/safe_archive_analyzer.mojom.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace {

// Prepares the file and temp file for analysis and returns the result on the UI
// thread with either `success_callback` or `failure_callback`.
void PrepareFileToAnalyze(
    base::FilePath file_path,
    base::OnceCallback<void(base::File file,
                            base::File temp_file,
                            base::File temp_file2)> success_callback,
    base::OnceCallback<void(safe_browsing::ArchiveAnalysisResult reason)>
        failure_callback) {
  base::File file(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                                 base::File::FLAG_WIN_SHARE_DELETE);

  if (!file.IsValid()) {
    DLOG(ERROR) << "Could not open file: " << file_path.value();
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(failure_callback),
                       safe_browsing::ArchiveAnalysisResult::kFailedToOpen));
    return;
  }

  base::FilePath temp_path, temp_path2;
  base::File temp_file, temp_file2;
  if (base::CreateTemporaryFile(&temp_path)) {
    temp_file.Initialize(
        temp_path, (base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_READ |
                    base::File::FLAG_WRITE | base::File::FLAG_WIN_TEMPORARY |
                    base::File::FLAG_DELETE_ON_CLOSE));
  }

  if (base::CreateTemporaryFile(&temp_path2)) {
    temp_file2.Initialize(
        temp_path2, (base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_READ |
                     base::File::FLAG_WRITE | base::File::FLAG_WIN_TEMPORARY |
                     base::File::FLAG_DELETE_ON_CLOSE));
  }

  if (!temp_file.IsValid()) {
    DLOG(ERROR) << "Could not open temp file: " << temp_path.value();
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(failure_callback),
            safe_browsing::ArchiveAnalysisResult::kFailedToOpenTempFile));
    return;
  }

  if (!temp_file2.IsValid()) {
    DLOG(ERROR) << "Could not open temp file: " << temp_path2.value();
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(failure_callback),
            safe_browsing::ArchiveAnalysisResult::kFailedToOpenTempFile));
    return;
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(success_callback), std::move(file),
                                std::move(temp_file), std::move(temp_file2)));
}

}  // namespace

// static
std::unique_ptr<SandboxedSevenZipAnalyzer, base::OnTaskRunnerDeleter>
SandboxedSevenZipAnalyzer::CreateAnalyzer(
    const base::FilePath& zip_file,
    ResultCallback callback,
    mojo::PendingRemote<chrome::mojom::FileUtilService> service) {
  return std::unique_ptr<SandboxedSevenZipAnalyzer, base::OnTaskRunnerDeleter>(
      new SandboxedSevenZipAnalyzer(zip_file, std::move(callback),
                                    std::move(service)),
      base::OnTaskRunnerDeleter(content::GetUIThreadTaskRunner({})));
}

SandboxedSevenZipAnalyzer::SandboxedSevenZipAnalyzer(
    const base::FilePath& zip_file,
    ResultCallback callback,
    mojo::PendingRemote<chrome::mojom::FileUtilService> service)
    : file_path_(zip_file),
      callback_(std::move(callback)),
      service_(std::move(service)) {
  DCHECK(callback_);
  service_->BindSafeArchiveAnalyzer(
      remote_analyzer_.BindNewPipeAndPassReceiver());
  remote_analyzer_.set_disconnect_handler(
      base::BindOnce(&SandboxedSevenZipAnalyzer::AnalyzeFileDone, GetWeakPtr(),
                     safe_browsing::ArchiveAnalyzerResults()));
}

void SandboxedSevenZipAnalyzer::Start() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(
          &PrepareFileToAnalyze, file_path_,
          base::BindOnce(&SandboxedSevenZipAnalyzer::AnalyzeFile, GetWeakPtr()),
          base::BindOnce(&SandboxedSevenZipAnalyzer::ReportFileFailure,
                         GetWeakPtr())));
}

SandboxedSevenZipAnalyzer::~SandboxedSevenZipAnalyzer() = default;

void SandboxedSevenZipAnalyzer::ReportFileFailure(
    safe_browsing::ArchiveAnalysisResult reason) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (callback_) {
    safe_browsing::ArchiveAnalyzerResults results;
    results.analysis_result = reason;

    std::move(callback_).Run(results);
  }
}

void SandboxedSevenZipAnalyzer::AnalyzeFile(base::File file,
                                            base::File temp_file,
                                            base::File temp_file2) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (remote_analyzer_) {
    remote_analyzer_->AnalyzeSevenZipFile(
        std::move(file), std::move(temp_file), std::move(temp_file2),
        base::BindOnce(&SandboxedSevenZipAnalyzer::AnalyzeFileDone,
                       GetWeakPtr()));
  } else {
    AnalyzeFileDone(safe_browsing::ArchiveAnalyzerResults());
  }
}

void SandboxedSevenZipAnalyzer::AnalyzeFileDone(
    const safe_browsing::ArchiveAnalyzerResults& results) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  remote_analyzer_.reset();
  if (callback_) {
    std::move(callback_).Run(results);
  }
}

base::WeakPtr<SandboxedSevenZipAnalyzer>
SandboxedSevenZipAnalyzer::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}
