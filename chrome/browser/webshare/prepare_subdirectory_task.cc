// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webshare/prepare_subdirectory_task.h"

#include "base/files/file.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace webshare {

PrepareSubDirectoryTask::PrepareSubDirectoryTask(
    std::vector<base::FilePath> subdirectories,
    blink::mojom::ShareService::ShareCallback callback)
    : subdirectories_(std::move(subdirectories)),
      callback_(std::move(callback)) {}

PrepareSubDirectoryTask::~PrepareSubDirectoryTask() = default;

void PrepareSubDirectoryTask::Start() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&PrepareSubDirectoryTask::PrepareSubDirectories,
                     subdirectories_),
      base::BindOnce(&PrepareSubDirectoryTask::OnPrepareSubDirectories,
                     weak_ptr_factory_.GetWeakPtr()));
}

// static
base::File::Error PrepareSubDirectoryTask::PrepareSubDirectories(
    std::vector<base::FilePath> subdirectories) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);

  base::File::Error result = base::File::FILE_OK;
  for (const auto& subdirectory : subdirectories) {
    if (!base::CreateDirectoryAndGetError(subdirectory.DirName(), &result)) {
      return result;
    }
  }
  return result;
}

void PrepareSubDirectoryTask::OnPrepareSubDirectories(
    base::File::Error result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::move(callback_).Run((result == base::File::FILE_OK)
                               ? blink::mojom::ShareError::OK
                               : blink::mojom::ShareError::PERMISSION_DENIED);
}

}  // namespace webshare
