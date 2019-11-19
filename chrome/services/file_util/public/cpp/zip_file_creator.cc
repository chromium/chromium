// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/file_util/public/cpp/zip_file_creator.h"

#include <utility>

#include "base/bind.h"
#include "base/task/post_task.h"
#include "components/services/filesystem/directory_impl.h"
#include "components/services/filesystem/lock_table.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace {

// Creates the destination zip file only if it does not already exist.
base::File OpenFileHandleAsync(const base::FilePath& zip_path) {
  return base::File(zip_path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
}

void BindDirectoryInBackground(
    const base::FilePath& src_dir,
    mojo::PendingReceiver<filesystem::mojom::Directory> receiver) {
  auto directory_impl = std::make_unique<filesystem::DirectoryImpl>(
      src_dir, /*temp_dir=*/nullptr, /*lock_table=*/nullptr);
  mojo::MakeSelfOwnedReceiver(std::move(directory_impl), std::move(receiver));
}

}  // namespace

ZipFileCreator::ZipFileCreator(
    ResultCallback callback,
    const base::FilePath& src_dir,
    const std::vector<base::FilePath>& src_relative_paths,
    const base::FilePath& dest_file)
    : callback_(std::move(callback)),
      src_dir_(src_dir),
      src_relative_paths_(src_relative_paths),
      dest_file_(dest_file) {
  DCHECK(callback_);
}

void ZipFileCreator::Start(
    mojo::PendingRemote<chrome::mojom::FileUtilService> service) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Note this class owns itself (it self-deletes when finished in ReportDone),
  // so it is safe to use base::Unretained(this).
  base::PostTaskAndReplyWithResult(
      FROM_HERE, {base::ThreadPool(), base::MayBlock()},
      base::BindOnce(&OpenFileHandleAsync, dest_file_),
      base::BindOnce(&ZipFileCreator::CreateZipFile, base::Unretained(this),
                     std::move(service)));
}

ZipFileCreator::~ZipFileCreator() = default;

void ZipFileCreator::CreateZipFile(
    mojo::PendingRemote<chrome::mojom::FileUtilService> service,
    base::File file) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!remote_zip_file_creator_);

  if (!file.IsValid()) {
    LOG(ERROR) << "Failed to create dest zip file " << dest_file_.value();
    ReportDone(false);
    return;
  }

  if (!directory_task_runner_) {
    directory_task_runner_ = base::CreateSequencedTaskRunner(
        {base::ThreadPool(), base::MayBlock(),
         base::TaskPriority::USER_BLOCKING,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  }

  mojo::PendingRemote<filesystem::mojom::Directory> directory;
  directory_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&BindDirectoryInBackground, src_dir_,
                                directory.InitWithNewPipeAndPassReceiver()));

  service_.Bind(std::move(service));
  service_->BindZipFileCreator(
      remote_zip_file_creator_.BindNewPipeAndPassReceiver());

  // See comment in Start() on why using base::Unretained(this) is safe.
  remote_zip_file_creator_.set_disconnect_handler(base::BindOnce(
      &ZipFileCreator::ReportDone, base::Unretained(this), false));
  remote_zip_file_creator_->CreateZipFile(
      std::move(directory), src_dir_, src_relative_paths_, std::move(file),
      base::BindOnce(&ZipFileCreator::ReportDone, base::Unretained(this)));
}

void ZipFileCreator::ReportDone(bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  remote_zip_file_creator_.reset();
  std::move(callback_).Run(success);

  delete this;
}
