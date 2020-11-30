// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webshare/chromeos/prepare_directory_task.h"

#include "base/files/file.h"
#include "base/system/sys_info.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/cros_system_api/constants/cryptohome.h"

using content::BrowserThread;

namespace webshare {

PrepareDirectoryTask::PrepareDirectoryTask(
    base::FilePath directory,
    uint64_t required_space,
    blink::mojom::ShareService::ShareCallback callback)
    : directory_(std::move(directory)),
      required_space_(required_space),
      callback_(std::move(callback)) {}

PrepareDirectoryTask::~PrepareDirectoryTask() = default;

void PrepareDirectoryTask::Start() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&PrepareDirectoryTask::PrepareDirectory, directory_,
                     required_space_),
      base::BindOnce(&PrepareDirectoryTask::OnPrepareDirectory,
                     weak_ptr_factory_.GetWeakPtr()));
}

// static
base::File::Error PrepareDirectoryTask::PrepareDirectory(
    base::FilePath directory,
    uint64_t required_space) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);

  base::File::Error result = base::File::FILE_OK;
  if (base::CreateDirectoryAndGetError(directory, &result)) {
    if (base::SysInfo::AmountOfFreeDiskSpace(directory) <
        static_cast<int64_t>(cryptohome::kMinFreeSpaceInBytes +
                             required_space)) {
      result = base::File::FILE_ERROR_NO_SPACE;
      VLOG(1) << "Insufficient space for sharing files";
    }
  } else {
    DCHECK(result != base::File::FILE_OK);
    VLOG(1) << "Could not create directory for shared files";
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
