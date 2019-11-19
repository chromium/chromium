// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/file_util/public/cpp/sandboxed_dmg_analyzer_mac.h"

#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "chrome/common/safe_browsing/archive_analyzer_results.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

SandboxedDMGAnalyzer::SandboxedDMGAnalyzer(
    const base::FilePath& dmg_file,
    const uint64_t max_size,
    const ResultCallback& callback,
    mojo::PendingRemote<chrome::mojom::FileUtilService> service)
    : file_path_(dmg_file),
      max_size_(max_size),
      callback_(callback),
      service_(std::move(service)) {
  DCHECK(callback);
  service_->BindSafeArchiveAnalyzer(
      remote_analyzer_.BindNewPipeAndPassReceiver());
  remote_analyzer_.set_disconnect_handler(base::BindOnce(
      &SandboxedDMGAnalyzer::AnalyzeFileDone, base::Unretained(this),
      safe_browsing::ArchiveAnalyzerResults()));
}

void SandboxedDMGAnalyzer::Start() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::PostTask(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&SandboxedDMGAnalyzer::PrepareFileToAnalyze, this));
}

SandboxedDMGAnalyzer::~SandboxedDMGAnalyzer() = default;

void SandboxedDMGAnalyzer::PrepareFileToAnalyze() {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  base::File file(file_path_, base::File::FLAG_OPEN | base::File::FLAG_READ);

  if (!file.IsValid()) {
    DLOG(ERROR) << "Could not open file: " << file_path_.value();
    ReportFileFailure();
    return;
  }

  uint64_t size = file.GetLength();

  bool too_big_to_unpack = base::checked_cast<uint64_t>(size) > max_size_;
  if (too_big_to_unpack) {
    DLOG(ERROR) << "File is too big: " << file_path_.value();
    ReportFileFailure();
    return;
  }

  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 base::BindOnce(&SandboxedDMGAnalyzer::AnalyzeFile, this,
                                std::move(file)));
}

void SandboxedDMGAnalyzer::ReportFileFailure() {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  base::PostTask(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(callback_, safe_browsing::ArchiveAnalyzerResults()));
}

void SandboxedDMGAnalyzer::AnalyzeFile(base::File file) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  remote_analyzer_->AnalyzeDmgFile(
      std::move(file),
      base::BindOnce(&SandboxedDMGAnalyzer::AnalyzeFileDone, this));
}

void SandboxedDMGAnalyzer::AnalyzeFileDone(
    const safe_browsing::ArchiveAnalyzerResults& results) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  remote_analyzer_.reset();
  callback_.Run(results);
}
