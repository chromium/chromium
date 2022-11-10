// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/file_util/public/cpp/sandboxed_document_analyzer.h"

#include <utility>
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/process/process_handle.h"
#include "base/task/thread_pool.h"
#include "chrome/common/safe_browsing/document_analyzer_results.h"
#include "chrome/services/file_util/public/mojom/safe_document_analyzer.mojom.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

SandboxedDocumentAnalyzer::SandboxedDocumentAnalyzer(
    const base::FilePath& target_document_path,
    const base::FilePath& tmp_document_path,
    ResultCallback callback,
    mojo::PendingRemote<chrome::mojom::DocumentAnalysisService> service)
    : RefCountedDeleteOnSequence(content::GetUIThreadTaskRunner({})),
      target_file_path_(target_document_path),
      tmp_file_path_(tmp_document_path),
      callback_(std::move(callback)),
      service_(std::move(service)) {
  DCHECK(callback_);
  DCHECK(!target_file_path_.value().empty());
  DCHECK(!tmp_file_path_.value().empty());
  service_->BindSafeDocumentAnalyzer(
      remote_analyzer_.BindNewPipeAndPassReceiver());
  remote_analyzer_.set_disconnect_handler(base::BindOnce(
      &SandboxedDocumentAnalyzer::AnalyzeDocumentDone, base::Unretained(this),
      safe_browsing::DocumentAnalyzerResults()));
}

void SandboxedDocumentAnalyzer::Start() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&SandboxedDocumentAnalyzer::PrepareFileToAnalyze, this));
}

SandboxedDocumentAnalyzer::~SandboxedDocumentAnalyzer() = default;

void SandboxedDocumentAnalyzer::PrepareFileToAnalyze() {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  base::File file(tmp_file_path_,
                  base::File::FLAG_OPEN | base::File::FLAG_READ);

  if (!file.IsValid()) {
    ReportFileFailure("Could not open file");
    return;
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&SandboxedDocumentAnalyzer::AnalyzeDocument,
                                this, std::move(file), target_file_path_));
}

void SandboxedDocumentAnalyzer::AnalyzeDocument(
    base::File file,
    const base::FilePath& file_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (remote_analyzer_) {
    remote_analyzer_->AnalyzeDocument(
        std::move(file), target_file_path_,
        base::BindOnce(&SandboxedDocumentAnalyzer::AnalyzeDocumentDone, this));
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
  if (callback_) {
    struct safe_browsing::DocumentAnalyzerResults failure_results =
        safe_browsing::DocumentAnalyzerResults();
    failure_results.success = false;
    failure_results.has_macros = false;
    failure_results.error_message = msg;
    failure_results.error_code =
        safe_browsing::ClientDownloadRequest::DocumentProcessingInfo::INTERNAL;

    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_), failure_results));
  }
}
