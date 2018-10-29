// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/file_util/public/cpp/sandboxed_dmg_analyzer_mac.h"

#include <utility>

#include "base/bind.h"
#include "base/task/post_task.h"
#include "chrome/common/safe_browsing/archive_analyzer_results.h"
#include "chrome/services/file_util/public/mojom/constants.mojom.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "services/service_manager/public/cpp/connector.h"

SandboxedDMGAnalyzer::SandboxedDMGAnalyzer(
    const base::FilePath& dmg_file,
    const ResultCallback& callback,
    service_manager::Connector* connector)
    : file_path_(dmg_file), callback_(callback), connector_(connector) {
  DCHECK(callback);
}

void SandboxedDMGAnalyzer::Start() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::PostTaskWithTraits(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::Bind(&SandboxedDMGAnalyzer::PrepareFileToAnalyze, this));
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

  base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::UI},
                           base::Bind(&SandboxedDMGAnalyzer::AnalyzeFile, this,
                                      base::Passed(&file)));
}

void SandboxedDMGAnalyzer::ReportFileFailure() {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!analyzer_ptr_);

  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::Bind(callback_, safe_browsing::ArchiveAnalyzerResults()));
}

void SandboxedDMGAnalyzer::AnalyzeFile(base::File file) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!analyzer_ptr_);

  connector_->BindInterface(chrome::mojom::kFileUtilServiceName,
                            mojo::MakeRequest(&analyzer_ptr_));
  analyzer_ptr_.set_connection_error_handler(
      base::Bind(&SandboxedDMGAnalyzer::AnalyzeFileDone, base::Unretained(this),
                 safe_browsing::ArchiveAnalyzerResults()));
  analyzer_ptr_->AnalyzeDmgFile(
      std::move(file),
      base::Bind(&SandboxedDMGAnalyzer::AnalyzeFileDone, this));
}

void SandboxedDMGAnalyzer::AnalyzeFileDone(
    const safe_browsing::ArchiveAnalyzerResults& results) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  analyzer_ptr_.reset();
  callback_.Run(results);
}
