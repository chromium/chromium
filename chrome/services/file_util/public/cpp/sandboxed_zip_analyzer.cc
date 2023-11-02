// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/file_util/public/cpp/sandboxed_zip_analyzer.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/thread_pool.h"
#include "chrome/common/safe_browsing/archive_analyzer_results.h"
#include "chrome/services/file_util/public/mojom/safe_archive_analyzer.mojom.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

SandboxedZipAnalyzer::SandboxedZipAnalyzer(
    const base::FilePath& zip_file,
    ResultCallback callback,
    mojo::PendingRemote<chrome::mojom::FileUtilService> service)
    : RefCountedDeleteOnSequence(content::GetUIThreadTaskRunner({})),
      file_path_(zip_file),
      callback_(std::move(callback)),
      service_(std::move(service)) {
  DCHECK(callback_);
  service_->BindSafeArchiveAnalyzer(
      remote_analyzer_.BindNewPipeAndPassReceiver());
  remote_analyzer_.set_disconnect_handler(base::BindOnce(
      &SandboxedZipAnalyzer::AnalyzeFileDone, base::Unretained(this),
      safe_browsing::ArchiveAnalyzerResults()));
}

void SandboxedZipAnalyzer::Start() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&SandboxedZipAnalyzer::PrepareFileToAnalyze, this));
}

SandboxedZipAnalyzer::~SandboxedZipAnalyzer() = default;

void SandboxedZipAnalyzer::PrepareFileToAnalyze() {
  base::File file(file_path_, base::File::FLAG_OPEN | base::File::FLAG_READ);

  if (!file.IsValid()) {
    DLOG(ERROR) << "Could not open file: " << file_path_.value();
    ReportFileFailure(safe_browsing::ArchiveAnalysisResult::kFailedToOpen);
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
    ReportFileFailure(
        safe_browsing::ArchiveAnalysisResult::kFailedToOpenTempFile);
    return;
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&SandboxedZipAnalyzer::AnalyzeFile, this,
                                std::move(file), std::move(temp_file)));
}

void SandboxedZipAnalyzer::ReportFileFailure(
    safe_browsing::ArchiveAnalysisResult reason) {
  if (callback_) {
    safe_browsing::ArchiveAnalyzerResults results;
    results.analysis_result = reason;

    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_), results));
  }
}

void SandboxedZipAnalyzer::AnalyzeFile(base::File file, base::File temp_file) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (remote_analyzer_) {
    remote_analyzer_->AnalyzeZipFile(
        std::move(file), std::move(temp_file),
        base::BindOnce(&SandboxedZipAnalyzer::AnalyzeFileDone, this));
  } else {
    AnalyzeFileDone(safe_browsing::ArchiveAnalyzerResults());
  }
}

void SandboxedZipAnalyzer::AnalyzeFileDone(
    const safe_browsing::ArchiveAnalyzerResults& results) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  remote_analyzer_.reset();
  if (callback_) {
    std::move(callback_).Run(results);
  }
}
