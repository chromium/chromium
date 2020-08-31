// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/holding_space_file_system_delegate.h"

#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/sequence_checker.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace ash {

// HoldingSpaceFileSystemDelegate::FileSystemWatcher ---------------------------

class HoldingSpaceFileSystemDelegate::FileSystemWatcher {
 public:
  explicit FileSystemWatcher(base::FilePathWatcher::Callback callback)
      : callback_(callback) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    DETACH_FROM_SEQUENCE(sequence_checker_);
  }

  FileSystemWatcher(const FileSystemWatcher&) = delete;
  FileSystemWatcher& operator=(const FileSystemWatcher&) = delete;
  ~FileSystemWatcher() { DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_); }

  void AddWatch(const base::FilePath& file_path) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (base::Contains(watchers_, file_path))
      return;
    watchers_[file_path] = std::make_unique<base::FilePathWatcher>();
    watchers_[file_path]->Watch(
        file_path, /*recursive=*/false,
        base::Bind(&FileSystemWatcher::OnFilePathChanged,
                   weak_factory_.GetWeakPtr()));
  }

  void RemoveWatch(const base::FilePath& file_path) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    watchers_.erase(file_path);
  }

  base::WeakPtr<FileSystemWatcher> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  void OnFilePathChanged(const base::FilePath& file_path, bool error) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(callback_, file_path, error));
  }

  SEQUENCE_CHECKER(sequence_checker_);
  base::FilePathWatcher::Callback callback_;
  std::map<base::FilePath, std::unique_ptr<base::FilePathWatcher>> watchers_;
  base::WeakPtrFactory<FileSystemWatcher> weak_factory_{this};
};

// HoldingSpaceFileSystemDelegate ----------------------------------------------

HoldingSpaceFileSystemDelegate::HoldingSpaceFileSystemDelegate(
    Profile* profile,
    HoldingSpaceModel* model,
    FileRemovedCallback file_removed_callback)
    : HoldingSpaceKeyedServiceDelegate(profile, model),
      file_removed_callback_(file_removed_callback),
      file_system_watcher_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT})) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

HoldingSpaceFileSystemDelegate::~HoldingSpaceFileSystemDelegate() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  weak_factory_.InvalidateWeakPtrs();
  file_system_watcher_runner_->DeleteSoon(FROM_HERE,
                                          file_system_watcher_.release());
}

void HoldingSpaceFileSystemDelegate::Init() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  file_system_watcher_ = std::make_unique<FileSystemWatcher>(
      base::Bind(&HoldingSpaceFileSystemDelegate::OnFilePathChanged,
                 weak_factory_.GetWeakPtr()));
}

// TODO(dmblack): Watch `item`'s parent directory instead of its backing file so
// that we can reuse the same watcher across multiple holding space items.
void HoldingSpaceFileSystemDelegate::OnHoldingSpaceItemAdded(
    const HoldingSpaceItem* item) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  AddWatch(item->file_path());
}

void HoldingSpaceFileSystemDelegate::OnHoldingSpaceItemRemoved(
    const HoldingSpaceItem* item) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  RemoveWatch(item->file_path());
}

void HoldingSpaceFileSystemDelegate::OnFilePathChanged(
    const base::FilePath& file_path,
    bool error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!error);

  // Currently `OnFilePathChanged()` is only called on removal of `file_path`.
  file_removed_callback_.Run(file_path);
}

void HoldingSpaceFileSystemDelegate::AddWatch(const base::FilePath& file_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  file_system_watcher_runner_->PostTask(
      FROM_HERE, base::BindOnce(&FileSystemWatcher::AddWatch,
                                file_system_watcher_->GetWeakPtr(), file_path));
}

void HoldingSpaceFileSystemDelegate::RemoveWatch(
    const base::FilePath& file_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  file_system_watcher_runner_->PostTask(
      FROM_HERE, base::BindOnce(&FileSystemWatcher::RemoveWatch,
                                file_system_watcher_->GetWeakPtr(), file_path));
}

}  // namespace ash
