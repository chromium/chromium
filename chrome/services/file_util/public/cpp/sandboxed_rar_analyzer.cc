// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/file_util/public/cpp/sandboxed_rar_analyzer.h"

#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/process/process_handle.h"
#include "base/strings/stringprintf.h"
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
    base::OnceCallback<void(base::File, base::File)> success_callback,
    base::OnceCallback<void(safe_browsing::ArchiveAnalysisResult reason)>
        failure_callback) {
  if (file_path.value().empty()) {
    // TODO(vakh): Add UMA metrics here to check how often this happens.
    DLOG(ERROR) << "file_path empty!";
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(failure_callback),
                       safe_browsing::ArchiveAnalysisResult::kFailedToOpen));
    return;
  }

  base::File file(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                                 base::File::FLAG_WIN_SHARE_DELETE);
  if (!file.IsValid()) {
    // TODO(vakh): Add UMA metrics here to check how often this happens.
    DLOG(ERROR) << "Could not open file: " << file_path.value();
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(failure_callback),
                       safe_browsing::ArchiveAnalysisResult::kFailedToOpen));
    return;
  }

  base::FilePath temp_path;
  base::File temp_file;
  if (base::CreateTemporaryFile(&temp_path)) {
    temp_file.Initialize(
        temp_path, (base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_READ |
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

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(success_callback), std::move(file),
                                std::move(temp_file)));
}

}  // namespace

// static
std::unique_ptr<SandboxedRarAnalyzer, base::OnTaskRunnerDeleter>
SandboxedRarAnalyzer::CreateAnalyzer(
    const base::FilePath& rar_file_path,
    ResultCallback callback,
    mojo::PendingRemote<chrome::mojom::FileUtilService> service) {
  return std::unique_ptr<SandboxedRarAnalyzer, base::OnTaskRunnerDeleter>(
      new SandboxedRarAnalyzer(rar_file_path, std::move(callback),
                               std::move(service)),
      base::OnTaskRunnerDeleter(content::GetUIThreadTaskRunner({})));
}

SandboxedRarAnalyzer::SandboxedRarAnalyzer(
    const base::FilePath& rar_file_path,
    ResultCallback callback,
    mojo::PendingRemote<chrome::mojom::FileUtilService> service)
    : file_path_(rar_file_path),
      callback_(std::move(callback)),
      service_(std::move(service)) {
  DCHECK(callback_);
  DCHECK(!file_path_.value().empty());
  service_->BindSafeArchiveAnalyzer(
      remote_analyzer_.BindNewPipeAndPassReceiver());
  remote_analyzer_.set_disconnect_handler(
      base::BindOnce(&SandboxedRarAnalyzer::AnalyzeFileDone, GetWeakPtr(),
                     safe_browsing::ArchiveAnalyzerResults()));
}

void SandboxedRarAnalyzer::Start() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(
          &PrepareFileToAnalyze, file_path_,
          base::BindOnce(&SandboxedRarAnalyzer::AnalyzeFile, GetWeakPtr()),
          base::BindOnce(&SandboxedRarAnalyzer::ReportFileFailure,
                         GetWeakPtr())));
}

SandboxedRarAnalyzer::~SandboxedRarAnalyzer() = default;

void SandboxedRarAnalyzer::AnalyzeFile(base::File file, base::File temp_file) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!file_path_.value().empty());
  if (remote_analyzer_) {
    remote_analyzer_->AnalyzeRarFile(
        std::move(file), std::move(temp_file),
        base::BindOnce(&SandboxedRarAnalyzer::AnalyzeFileDone, GetWeakPtr()));
  } else {
    AnalyzeFileDone(safe_browsing::ArchiveAnalyzerResults());
  }
}

void SandboxedRarAnalyzer::AnalyzeFileDone(
    const safe_browsing::ArchiveAnalyzerResults& results) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  remote_analyzer_.reset();
  if (callback_) {
    std::move(callback_).Run(results);
  }
}

void SandboxedRarAnalyzer::ReportFileFailure(
    safe_browsing::ArchiveAnalysisResult reason) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (callback_) {
    safe_browsing::ArchiveAnalyzerResults results;
    results.analysis_result = reason;
    std::move(callback_).Run(results);
  }
}

std::string SandboxedRarAnalyzer::DebugString() const {
  return base::StringPrintf("path: %" PRFilePath "; connected_: %d",
                            file_path_.value().c_str(),
                            remote_analyzer_.is_connected());
}

std::ostream& operator<<(std::ostream& os,
                         const SandboxedRarAnalyzer& rar_analyzer) {
  os << rar_analyzer.DebugString();
  return os;
}

base::WeakPtr<SandboxedRarAnalyzer> SandboxedRarAnalyzer::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}
