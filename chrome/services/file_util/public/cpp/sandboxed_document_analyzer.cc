// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/file_util/public/cpp/sandboxed_document_analyzer.h"

#include <utility>
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/process/process_handle.h"
#include "base/task/thread_pool.h"
#include "chrome/common/safe_browsing/document_analyzer_results.h"
#include "chrome/services/file_util/public/mojom/safe_document_analyzer.mojom.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace {

// Prepares the file for analysis and returns the result on the UI thread with
// either `success_callback` or `failure_callback`.
void PrepareFileToAnalyze(
    base::FilePath file_path,
    base::OnceCallback<void(base::File)> success_callback,
    base::OnceCallback<void(std::string)> failure_callback) {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  base::File file(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                                 base::File::FLAG_WIN_SHARE_DELETE);

  if (!file.IsValid()) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(failure_callback), "Could not open file"));
    return;
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(success_callback), std::move(file)));
}

}  // namespace

// static
std::unique_ptr<SandboxedDocumentAnalyzer, base::OnTaskRunnerDeleter>
SandboxedDocumentAnalyzer::CreateAnalyzer(
    const base::FilePath& target_document_path,
    const base::FilePath& tmp_document_path,
    ResultCallback callback,
    mojo::PendingRemote<chrome::mojom::DocumentAnalysisService> service) {
  return std::unique_ptr<SandboxedDocumentAnalyzer, base::OnTaskRunnerDeleter>(
      new SandboxedDocumentAnalyzer(target_document_path, tmp_document_path,
                                    std::move(callback), std::move(service)),
      base::OnTaskRunnerDeleter(content::GetUIThreadTaskRunner({})));
}

SandboxedDocumentAnalyzer::SandboxedDocumentAnalyzer(
    const base::FilePath& target_document_path,
    const base::FilePath& tmp_document_path,
    ResultCallback callback,
    mojo::PendingRemote<chrome::mojom::DocumentAnalysisService> service)
    : target_file_path_(target_document_path),
      tmp_file_path_(tmp_document_path),
      callback_(std::move(callback)),
      service_(std::move(service)) {
  DCHECK(callback_);
  DCHECK(!target_file_path_.value().empty());
  DCHECK(!tmp_file_path_.value().empty());
  service_->BindSafeDocumentAnalyzer(
      remote_analyzer_.BindNewPipeAndPassReceiver());
  remote_analyzer_.set_disconnect_handler(
      base::BindOnce(&SandboxedDocumentAnalyzer::AnalyzeDocumentDone,
                     GetWeakPtr(), safe_browsing::DocumentAnalyzerResults()));
}

void SandboxedDocumentAnalyzer::Start() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(
          &PrepareFileToAnalyze, tmp_file_path_,
          base::BindOnce(&SandboxedDocumentAnalyzer::AnalyzeDocument,
                         GetWeakPtr()),
          base::BindOnce(&SandboxedDocumentAnalyzer::ReportFileFailure,
                         GetWeakPtr())));
}

SandboxedDocumentAnalyzer::~SandboxedDocumentAnalyzer() = default;

void SandboxedDocumentAnalyzer::AnalyzeDocument(base::File file) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (remote_analyzer_) {
    remote_analyzer_->AnalyzeDocument(
        std::move(file), target_file_path_,
        base::BindOnce(&SandboxedDocumentAnalyzer::AnalyzeDocumentDone,
                       GetWeakPtr()));
  } else {
    AnalyzeDocumentDone(safe_browsing::DocumentAnalyzerResults());
  }
}

void SandboxedDocumentAnalyzer::AnalyzeDocumentDone(
    const safe_browsing::DocumentAnalyzerResults& results) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  remote_analyzer_.reset();
  if (callback_) {
    std::move(callback_).Run(results);
  }
}

void SandboxedDocumentAnalyzer::ReportFileFailure(const std::string msg) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (callback_) {
    struct safe_browsing::DocumentAnalyzerResults failure_results =
        safe_browsing::DocumentAnalyzerResults();
    failure_results.success = false;
    failure_results.has_macros = false;
    failure_results.error_message = msg;
    failure_results.error_code =
        safe_browsing::ClientDownloadRequest::DocumentProcessingInfo::INTERNAL;

    std::move(callback_).Run(failure_results);
  }
}

base::WeakPtr<SandboxedDocumentAnalyzer>
SandboxedDocumentAnalyzer::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}
