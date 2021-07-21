// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/file_util/public/cpp/zip_file_creator.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "components/services/filesystem/directory_impl.h"
#include "components/services/filesystem/lock_table.h"
#include "content/public/browser/browser_thread.h"

namespace {

std::string Redact(const std::string& s) {
  return LOG_IS_ON(INFO) ? base::StrCat({"'", s, "'"}) : "(redacted)";
}

std::string Redact(const base::FilePath& path) {
  return Redact(path.value());
}

// Creates the destination zip file only if it does not already exist.
base::File OpenFileHandleAsync(const base::FilePath& zip_path) {
  return base::File(zip_path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
}

}  // namespace

ZipFileCreator::ZipFileCreator(ResultCallback result_callback,
                               base::FilePath src_dir,
                               std::vector<base::FilePath> src_relative_paths,
                               base::FilePath dest_file)
    : result_callback_(std::move(result_callback)),
      src_dir_(std::move(src_dir)),
      src_relative_paths_(std::move(src_relative_paths)),
      dest_file_(std::move(dest_file)) {
  DCHECK(result_callback_);
}

ZipFileCreator::~ZipFileCreator() {
  DCHECK(!result_callback_);
  DCHECK(!remote_zip_file_creator_);
}

void ZipFileCreator::Start(
    mojo::PendingRemote<chrome::mojom::FileUtilService> service) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&OpenFileHandleAsync, dest_file_),
      base::BindOnce(&ZipFileCreator::CreateZipFile, this, std::move(service)));
}

void ZipFileCreator::Stop() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ReportResult(kCancelled);
}

void ZipFileCreator::CreateZipFile(
    mojo::PendingRemote<chrome::mojom::FileUtilService> service,
    base::File file) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!remote_zip_file_creator_);

  if (!file.IsValid()) {
    LOG(ERROR) << "Cannot create ZIP file " << Redact(dest_file_);
    ReportResult(kError);
    return;
  }

  mojo::PendingRemote<filesystem::mojom::Directory> directory;
  BindDirectory(directory.InitWithNewPipeAndPassReceiver());

  service_.Bind(std::move(service));
  service_->BindZipFileCreator(
      remote_zip_file_creator_.BindNewPipeAndPassReceiver());

  remote_zip_file_creator_.set_disconnect_handler(
      base::BindOnce(&ZipFileCreator::ReportResult, this, kError));

  remote_zip_file_creator_->CreateZipFile(
      std::move(directory), src_relative_paths_, std::move(file),
      listener_.BindNewPipeAndPassRemote(),
      base::BindOnce(&ZipFileCreator::OnFinished, this));
}

void ZipFileCreator::BindDirectory(
    mojo::PendingReceiver<filesystem::mojom::Directory> receiver) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  using RunnerPtr = scoped_refptr<base::SequencedTaskRunner>;
  const RunnerPtr runner = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

  runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::FilePath src_dir,
             mojo::PendingReceiver<filesystem::mojom::Directory> receiver,
             RunnerPtr runner) {
            mojo::MakeSelfOwnedReceiver(
                std::make_unique<filesystem::DirectoryImpl>(
                    std::move(src_dir), /*temp_dir=*/nullptr,
                    /*lock_table=*/nullptr),
                std::move(receiver), std::move(runner));
          },
          src_dir_, std::move(receiver), runner));
}

void ZipFileCreator::OnFinished(const bool success) {
  ReportResult(success ? kSuccess : kError);
}

void ZipFileCreator::ReportResult(const Result result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::UmaHistogramEnumeration("ZipFileCreator.Result", result);

  listener_.reset();
  remote_zip_file_creator_.reset();

  const bool success = result == kSuccess;
  // In case of error, remove the partially created ZIP file.
  if (!success)
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(base::GetDeleteFileCallback(), dest_file_));

  if (result_callback_)
    std::move(result_callback_).Run(success);
}

void ZipFileCreator::OnProgress(const uint64_t bytes,
                                const uint32_t files,
                                const uint32_t directories) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // TODO(fdegros) Do something with progress information
  VLOG(0) << "ZIP progress: " << bytes << " bytes, " << files << " files, "
          << directories << " directories";
}
