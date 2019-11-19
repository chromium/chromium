// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/file_util/public/cpp/sandboxed_rar_analyzer.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/process/process_handle.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "chrome/common/safe_browsing/archive_analyzer_results.h"
#include "chrome/services/file_util/public/mojom/safe_archive_analyzer.mojom.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

SandboxedRarAnalyzer::SandboxedRarAnalyzer(
    const base::FilePath& rar_file_path,
    const ResultCallback& callback,
    mojo::PendingRemote<chrome::mojom::FileUtilService> service)
    : file_path_(rar_file_path),
      callback_(callback),
      service_(std::move(service)) {
  DCHECK(callback);
  DCHECK(!file_path_.value().empty());
  service_->BindSafeArchiveAnalyzer(
      remote_analyzer_.BindNewPipeAndPassReceiver());
  remote_analyzer_.set_disconnect_handler(base::BindOnce(
      &SandboxedRarAnalyzer::AnalyzeFileDone, base::Unretained(this),
      safe_browsing::ArchiveAnalyzerResults()));
}

void SandboxedRarAnalyzer::Start() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::PostTask(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&SandboxedRarAnalyzer::PrepareFileToAnalyze, this));
}

SandboxedRarAnalyzer::~SandboxedRarAnalyzer() = default;

void SandboxedRarAnalyzer::AnalyzeFile(base::File file, base::File temp_file) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!file_path_.value().empty());
  remote_analyzer_->AnalyzeRarFile(
      std::move(file), std::move(temp_file),
      base::BindOnce(&SandboxedRarAnalyzer::AnalyzeFileDone, this));
}

void SandboxedRarAnalyzer::AnalyzeFileDone(
    const safe_browsing::ArchiveAnalyzerResults& results) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  remote_analyzer_.reset();
  callback_.Run(results);
}

void SandboxedRarAnalyzer::PrepareFileToAnalyze() {
  if (file_path_.value().empty()) {
    // TODO(vakh): Add UMA metrics here to check how often this happens.
    DLOG(ERROR) << "file_path_ empty!";
    ReportFileFailure();
    return;
  }

  base::File file(file_path_, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    // TODO(vakh): Add UMA metrics here to check how often this happens.
    DLOG(ERROR) << "Could not open file: " << file_path_.value();
    ReportFileFailure();
    return;
  }

  base::FilePath temp_path;
  base::File temp_file;
  if (base::CreateTemporaryFile(&temp_path)) {
    temp_file.Initialize(
        temp_path, (base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_READ |
                    base::File::FLAG_WRITE | base::File::FLAG_TEMPORARY |
                    base::File::FLAG_DELETE_ON_CLOSE));
  }

  if (!temp_file.IsValid()) {
    DLOG(ERROR) << "Could not open temp file: " << temp_path.value();
    ReportFileFailure();
    return;
  }

  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 base::BindOnce(&SandboxedRarAnalyzer::AnalyzeFile, this,
                                std::move(file), std::move(temp_file)));
}

void SandboxedRarAnalyzer::ReportFileFailure() {
  base::PostTask(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(callback_, safe_browsing::ArchiveAnalyzerResults()));
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
