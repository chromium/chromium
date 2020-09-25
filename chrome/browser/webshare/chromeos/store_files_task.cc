// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webshare/chromeos/store_files_task.h"

#include <memory>

#include "base/bind.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/blob/blob_storage_context.h"

using content::BrowserThread;

namespace webshare {

StoreFilesTask::StoreFilesTask(
    content::BrowserContext::BlobContextGetter blob_context_getter,
    std::vector<base::FilePath> filenames,
    std::vector<blink::mojom::SharedFilePtr> files,
    blink::mojom::ShareService::ShareCallback callback)
    : blob_context_getter_(std::move(blob_context_getter)),
      filenames_(std::move(filenames)),
      files_(std::move(files)),
      callback_(std::move(callback)),
      index_(files_.size()) {
  DCHECK_EQ(filenames_.size(), files_.size());
  DCHECK(!files_.empty());
}

StoreFilesTask::~StoreFilesTask() = default;

void StoreFilesTask::Start() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(crbug.com/1132202): Limit the total size of shared files to
  // kMaxSharedFileBytes.

  // The StoreFilesTask is self-owned. It is deleted in OnProgress.
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&StoreFilesTask::OnProgress, base::Unretained(this),
                     storage::mojom::WriteBlobToFileResult::kSuccess));
}

void StoreFilesTask::OnProgress(storage::mojom::WriteBlobToFileResult result) {
  blink::mojom::ShareError share_result = blink::mojom::ShareError::OK;
  storage::mojom::BlobStorageContext* const blob_storage_context =
      blob_context_getter_.Run().get();
  if (!blob_storage_context ||
      result != storage::mojom::WriteBlobToFileResult::kSuccess) {
    share_result = blink::mojom::ShareError::PERMISSION_DENIED;
    index_ = 0U;
  }

  if (index_ == 0U) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_), share_result));

    delete this;
    return;
  }

  --index_;
  blob_storage_context->WriteBlobToFile(
      std::move(files_[index_]->blob->blob), filenames_[index_],
      /*flush_on_write=*/true,
      /*last_modified=*/base::nullopt,
      base::BindOnce(&StoreFilesTask::OnProgress, base::Unretained(this)));
}

}  // namespace webshare
