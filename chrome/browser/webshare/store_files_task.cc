// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webshare/store_files_task.h"

#include "base/functional/bind.h"
#include "base/task/thread_pool.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace webshare {

StoreFilesTask::StoreFilesTask(
    std::vector<base::FilePath> filenames,
    std::vector<blink::mojom::SharedFilePtr> files,
    uint64_t available_space,
    blink::mojom::ShareService::ShareCallback callback)
    : filenames_(std::move(filenames)),
      files_(std::move(files)),
      available_space_(available_space),
      callback_(std::move(callback)),
      index_(files_.size()) {
  DCHECK_EQ(filenames_.size(), files_.size());
  DCHECK(!files_.empty());
}

StoreFilesTask::~StoreFilesTask() = default;

void StoreFilesTask::Start() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // The tasks posted to this sequenced task runner do synchronous File I/O.
  file_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE});

  // The StoreFilesTask is self-owned. It is deleted in OnStoreFile.
  file_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&StoreFilesTask::OnStoreFile, base::Unretained(this),
                     blink::mojom::ShareError::OK));
}

void StoreFilesTask::OnStoreFile(blink::mojom::ShareError result) {
  DCHECK(file_task_runner_->RunsTasksInCurrentSequence());

  store_file_task_.reset();
  if (result != blink::mojom::ShareError::OK) {
    index_ = 0U;
  }

  if (index_ == 0U) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_), result));

    delete this;
    return;
  }

  --index_;
  // The StoreFilesTask is self-owned. It is deleted in OnStoreFile.
  store_file_task_ = std::make_unique<StoreFileTask>(
      std::move(filenames_[index_]), std::move(files_[index_]),
      available_space_,
      base::BindOnce(&StoreFilesTask::OnStoreFile, base::Unretained(this)));
  store_file_task_->Start();
}

}  // namespace webshare
