// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/file_util/public/cpp/sandboxed_dmg_analyzer_mac.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/common/safe_browsing/archive_analyzer_results.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace {

// Prepares the file for analysis and returns the result on the UI thread with
// either `success_callback` or `failure_callback`.
void PrepareFileToAnalyze(
    base::FilePath file_path,
    uint64_t max_size,
    base::OnceCallback<void(SandboxedDMGAnalyzer::WrappedFilePtr)>
        success_callback,
    base::OnceCallback<void(safe_browsing::ArchiveAnalysisResult)>
        failure_callback) {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  SandboxedDMGAnalyzer::WrappedFilePtr file(
      new base::File(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                                    base::File::FLAG_WIN_SHARE_DELETE),
      base::OnTaskRunnerDeleter(
          base::SequencedTaskRunner::GetCurrentDefault()));

  if (!file->IsValid()) {
    DLOG(ERROR) << "Could not open file: " << file_path.value();
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(failure_callback),
            std::move(safe_browsing::ArchiveAnalysisResult::kFailedToOpen)));
    return;
  }

  uint64_t size = file->GetLength();

  bool too_big_to_unpack = base::checked_cast<uint64_t>(size) > max_size;
  if (too_big_to_unpack) {
    DLOG(ERROR) << "File is too big: " << file_path.value();
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(failure_callback),
            std::move(safe_browsing::ArchiveAnalysisResult::kTooLarge)));
    return;
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(success_callback), std::move(file)));
}

}  // namespace

// static
std::unique_ptr<SandboxedDMGAnalyzer, base::OnTaskRunnerDeleter>
SandboxedDMGAnalyzer::CreateAnalyzer(
    const base::FilePath& dmg_file,
    const uint64_t max_size,
    ResultCallback callback,
    mojo::PendingRemote<chrome::mojom::FileUtilService> service) {
  return std::unique_ptr<SandboxedDMGAnalyzer, base::OnTaskRunnerDeleter>(
      new SandboxedDMGAnalyzer(dmg_file, max_size, std::move(callback),
                               std::move(service)),
      base::OnTaskRunnerDeleter(content::GetUIThreadTaskRunner({})));
}

SandboxedDMGAnalyzer::SandboxedDMGAnalyzer(
    const base::FilePath& dmg_file,
    const uint64_t max_size,
    ResultCallback callback,
    mojo::PendingRemote<chrome::mojom::FileUtilService> service)
    : file_path_(dmg_file),
      max_size_(max_size),
      callback_(std::move(callback)),
      service_(std::move(service)),
      file_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {
  DCHECK(callback_);
  service_->BindSafeArchiveAnalyzer(
      remote_analyzer_.BindNewPipeAndPassReceiver());
  remote_analyzer_.set_disconnect_handler(
      base::BindOnce(&SandboxedDMGAnalyzer::AnalyzeFileDone, GetWeakPtr(),
                     safe_browsing::ArchiveAnalyzerResults()));
}

void SandboxedDMGAnalyzer::Start() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  file_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &PrepareFileToAnalyze, file_path_, max_size_,
          base::BindOnce(&SandboxedDMGAnalyzer::AnalyzeFile, GetWeakPtr()),
          base::BindOnce(&SandboxedDMGAnalyzer::ReportFileFailure,
                         GetWeakPtr())));
}

SandboxedDMGAnalyzer::~SandboxedDMGAnalyzer() = default;

void SandboxedDMGAnalyzer::ReportFileFailure(
    safe_browsing::ArchiveAnalysisResult reason) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  if (callback_) {
    safe_browsing::ArchiveAnalyzerResults results;
    results.analysis_result = reason;
    std::move(callback_).Run(results);
  }
}

void SandboxedDMGAnalyzer::AnalyzeFile(
    SandboxedDMGAnalyzer::WrappedFilePtr file) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (remote_analyzer_) {
    mojo::PendingRemote<chrome::mojom::TemporaryFileGetter>
        temp_file_getter_remote =
            temp_file_getter_.GetRemoteTemporaryFileGetter();
    remote_analyzer_->AnalyzeDmgFile(
        std::move(*file), std::move(temp_file_getter_remote),
        base::BindOnce(&SandboxedDMGAnalyzer::AnalyzeFileDone, GetWeakPtr()));
  } else {
    AnalyzeFileDone(safe_browsing::ArchiveAnalyzerResults());
  }
}

void SandboxedDMGAnalyzer::AnalyzeFileDone(
    const safe_browsing::ArchiveAnalyzerResults& results) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  remote_analyzer_.reset();
  std::move(callback_).Run(results);
}

base::WeakPtr<SandboxedDMGAnalyzer> SandboxedDMGAnalyzer::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}
