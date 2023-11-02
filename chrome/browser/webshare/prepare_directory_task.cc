// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webshare/prepare_directory_task.h"

#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/system/sys_info.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"
#include "content/public/browser/browser_thread.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "third_party/cros_system_api/constants/cryptohome.h"
#endif

using content::BrowserThread;

namespace {

void DeleteSharedFiles(std::vector<base::FilePath> file_paths) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  for (const base::FilePath& name : file_paths) {
    base::DeletePathRecursively(name);
  }
}

}  // namespace

namespace webshare {

constexpr base::TimeDelta PrepareDirectoryTask::kSharedFileLifetime;

PrepareDirectoryTask::PrepareDirectoryTask(base::FilePath directory,
                                           uint64_t required_space)
    : directory_(std::move(directory)), required_space_(required_space) {}

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

void PrepareDirectoryTask::StartWithCallback(
    PrepareDirectoryCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&PrepareDirectoryTask::PrepareDirectory, directory_,
                     required_space_),
      std::move(callback));
}

// static
void PrepareDirectoryTask::ScheduleSharedFileDeletion(
    std::vector<base::FilePath> file_paths,
    base::TimeDelta delay) {
  base::ThreadPool::PostDelayedTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&DeleteSharedFiles, std::move(file_paths)), delay);
}

// static
base::File::Error PrepareDirectoryTask::PrepareDirectory(
    base::FilePath directory,
    uint64_t required_space) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);

  base::File::Error result = base::File::FILE_OK;
  if (base::CreateDirectoryAndGetError(directory, &result)) {
    // Delete any old files in |directory|.
    const base::Time cutoff_time = base::Time::Now() - kSharedFileLifetime;
    base::FileEnumerator enumerator(
        directory, /*recursive=*/false,
        base::FileEnumerator::DIRECTORIES | base::FileEnumerator::FILES);
    for (base::FilePath name = enumerator.Next(); !name.empty();
         name = enumerator.Next()) {
      if (enumerator.GetInfo().GetLastModifiedTime() <= cutoff_time) {
        base::DeletePathRecursively(name);
      }
    }

#if BUILDFLAG(IS_CHROMEOS)
    if (base::SysInfo::AmountOfFreeDiskSpace(directory) <
        static_cast<int64_t>(cryptohome::kMinFreeSpaceInBytes +
                             required_space)) {
#elif BUILDFLAG(IS_MAC)
    if (base::SysInfo::AmountOfFreeDiskSpace(directory) <
        static_cast<int64_t>(required_space)) {
#else
    if (false) {
#endif
      result = base::File::FILE_ERROR_NO_SPACE;
      VLOG(1) << "Insufficient space for sharing files";
    }
  } else {
    DCHECK(result != base::File::FILE_OK);
    VLOG(1) << "Could not create directory for shared files";
  }

  return result;
}

void PrepareDirectoryTask::OnPrepareDirectory(base::File::Error result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (callback_) {
    std::move(callback_).Run((result == base::File::FILE_OK)
                                 ? blink::mojom::ShareError::OK
                                 : blink::mojom::ShareError::PERMISSION_DENIED);
  }
}

}  // namespace webshare
