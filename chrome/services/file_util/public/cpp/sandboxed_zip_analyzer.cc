// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/file_util/public/cpp/sandboxed_zip_analyzer.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/common/safe_browsing/archive_analyzer_results.h"
#include "chrome/services/file_util/public/mojom/safe_archive_analyzer.mojom.h"
#include "components/enterprise/obfuscation/core/obfuscated_file_reader.h"
#include "components/enterprise/obfuscation/core/utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace {

// Prepares the file and temporary file for analysis, then returns the results
// with either `success_callback` or `failure_callback` on the UI thread.
void PrepareFileToAnalyze(
    base::FilePath file_path,
    bool is_obfuscated,
    base::OnceCallback<void(SandboxedZipAnalyzer::WrappedFilePtr,
                            std::optional<enterprise_obfuscation::HeaderData>)>
        success_callback,
    base::OnceCallback<void(safe_browsing::ArchiveAnalysisResult)>
        failure_callback) {
  SandboxedZipAnalyzer::WrappedFilePtr file(
      new base::File(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                                    base::File::FLAG_WIN_SHARE_DELETE),
      base::OnTaskRunnerDeleter(
          base::SequencedTaskRunner::GetCurrentDefault()));
  if (!file->IsValid()) {
    DLOG(ERROR) << "Could not open file: " << file_path.value();
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(failure_callback),
                       safe_browsing::ArchiveAnalysisResult::kFailedToOpen));
    return;
  }

  std::optional<enterprise_obfuscation::HeaderData> header_data;
  if (is_obfuscated) {
    auto header_result =
        enterprise_obfuscation::ObfuscatedFileReader::ReadHeaderData(*file);
    if (!header_result.has_value()) {
      DLOG(ERROR) << "Could not read obfuscated file header: "
                  << file_path.value();
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(failure_callback),
                         safe_browsing::ArchiveAnalysisResult::kFailedToOpen));
      return;
    }
    header_data = std::move(header_result.value());
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(success_callback), std::move(file),
                                std::move(header_data)));
}

}  // namespace

// static
std::unique_ptr<SandboxedZipAnalyzer, base::OnTaskRunnerDeleter>
SandboxedZipAnalyzer::CreateAnalyzer(
    const base::FilePath& zip_file,
    base::optional_ref<const std::string> password,
    ResultCallback callback,
    mojo::PendingRemote<chrome::mojom::FileUtilService> service) {
  return std::unique_ptr<SandboxedZipAnalyzer, base::OnTaskRunnerDeleter>(
      new SandboxedZipAnalyzer(zip_file, password,
                               /*is_obfuscated_file=*/false,
                               std::move(callback), std::move(service)),
      base::OnTaskRunnerDeleter(content::GetUIThreadTaskRunner({})));
}

// static
std::unique_ptr<SandboxedZipAnalyzer, base::OnTaskRunnerDeleter>
SandboxedZipAnalyzer::CreateObfuscatedAnalyzer(
    const base::FilePath& zip_file,
    base::optional_ref<const std::string> password,
    ResultCallback callback,
    mojo::PendingRemote<chrome::mojom::FileUtilService> service) {
  return std::unique_ptr<SandboxedZipAnalyzer, base::OnTaskRunnerDeleter>(
      new SandboxedZipAnalyzer(zip_file, password, /*is_obfuscated_file=*/true,
                               std::move(callback), std::move(service)),
      base::OnTaskRunnerDeleter(content::GetUIThreadTaskRunner({})));
}

SandboxedZipAnalyzer::SandboxedZipAnalyzer(
    const base::FilePath& zip_file,
    base::optional_ref<const std::string> password,
    bool is_obfuscated_file,
    ResultCallback callback,
    mojo::PendingRemote<chrome::mojom::FileUtilService> service)
    : file_path_(zip_file),
      password_(password.CopyAsOptional()),
      is_obfuscated_file_(is_obfuscated_file),
      callback_(std::move(callback)),
      service_(std::move(service)),
      file_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {
  DCHECK(callback_);
  service_->BindSafeArchiveAnalyzer(
      remote_analyzer_.BindNewPipeAndPassReceiver());
  remote_analyzer_.set_disconnect_handler(
      base::BindOnce(&SandboxedZipAnalyzer::AnalyzeFileDone, GetWeakPtr(),
                     safe_browsing::ArchiveAnalyzerResults()));
}

void SandboxedZipAnalyzer::Start() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  file_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &PrepareFileToAnalyze, file_path_, is_obfuscated_file_,
          base::BindOnce(&SandboxedZipAnalyzer::AnalyzeFile, GetWeakPtr()),
          base::BindOnce(&SandboxedZipAnalyzer::ReportFileFailure,
                         GetWeakPtr())));
}

SandboxedZipAnalyzer::~SandboxedZipAnalyzer() = default;

void SandboxedZipAnalyzer::ReportFileFailure(
    safe_browsing::ArchiveAnalysisResult reason) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (callback_) {
    safe_browsing::ArchiveAnalyzerResults results;
    results.analysis_result = reason;

    std::move(callback_).Run(results);
  }
}

void SandboxedZipAnalyzer::AnalyzeFile(
    WrappedFilePtr file,
    std::optional<enterprise_obfuscation::HeaderData> header_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (remote_analyzer_) {
    mojo::PendingRemote<chrome::mojom::TemporaryFileGetter>
        temp_file_getter_remote =
            temp_file_getter_.GetRemoteTemporaryFileGetter();
    if (header_data.has_value()) {
      auto header_data_ptr = chrome::mojom::ObfuscatedFileUtilHeaderData::New();
      header_data_ptr->derived_key.assign(header_data->derived_key.begin(),
                                          header_data->derived_key.end());
      header_data_ptr->nonce_prefix = header_data->nonce_prefix;
      remote_analyzer_->AnalyzeObfuscatedZipFile(
          std::move(*file), password_, std::move(header_data_ptr),
          std::move(temp_file_getter_remote),
          base::BindOnce(&SandboxedZipAnalyzer::AnalyzeFileDone, GetWeakPtr()));
    } else {
      remote_analyzer_->AnalyzeZipFile(
          std::move(*file), password_, std::move(temp_file_getter_remote),
          base::BindOnce(&SandboxedZipAnalyzer::AnalyzeFileDone, GetWeakPtr()));
    }
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

base::WeakPtr<SandboxedZipAnalyzer> SandboxedZipAnalyzer::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}
