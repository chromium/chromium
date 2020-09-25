// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webshare/chromeos/prepare_directory_task.h"

#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace webshare {

PrepareDirectoryTask::PrepareDirectoryTask(
    base::FilePath directory,
    blink::mojom::ShareService::ShareCallback callback)
    : directory_(std::move(directory)), callback_(std::move(callback)) {}

PrepareDirectoryTask::~PrepareDirectoryTask() = default;

void PrepareDirectoryTask::Start() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&PrepareDirectoryTask::PrepareDirectory, directory_),
      base::BindOnce(&PrepareDirectoryTask::OnPrepareDirectory,
                     weak_ptr_factory_.GetWeakPtr()));
}

// static
base::File::Error PrepareDirectoryTask::PrepareDirectory(
    base::FilePath directory) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);

  base::File::Error result = base::File::FILE_OK;
  if (!base::CreateDirectoryAndGetError(directory, &result)) {
    DCHECK(result != base::File::FILE_OK);
  }

  // TODO(crbug.com/1110119): Delete any old files in directory.

  return result;
}

void PrepareDirectoryTask::OnPrepareDirectory(base::File::Error result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::move(callback_).Run((result == base::File::FILE_OK)
                               ? blink::mojom::ShareError::OK
                               : blink::mojom::ShareError::PERMISSION_DENIED);
}

}  // namespace webshare
