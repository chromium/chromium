// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_FILE_SYSTEM_DELEGATE_H_
#define CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_FILE_SYSTEM_DELEGATE_H_

#include <memory>

#include "base/callback.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service_delegate.h"

namespace base {
class FilePath;
}  // namespace base

namespace ash {

// A delegate of `HoldingSpaceKeyedService` tasked with monitoring the file
// system for removal of files backing holding space items.
class HoldingSpaceFileSystemDelegate : public HoldingSpaceKeyedServiceDelegate {
 public:
  // Callback to be invoked when a watched file path is removed. The delegate
  // will watch file paths for all holding space items in the model.
  using FileRemovedCallback =
      base::RepeatingCallback<void(const base::FilePath&)>;

  HoldingSpaceFileSystemDelegate(Profile* profile,
                                 HoldingSpaceModel* model,
                                 FileRemovedCallback file_removed_callback);
  HoldingSpaceFileSystemDelegate(const HoldingSpaceFileSystemDelegate&) =
      delete;
  HoldingSpaceFileSystemDelegate& operator=(
      const HoldingSpaceFileSystemDelegate&) = delete;
  ~HoldingSpaceFileSystemDelegate() override;

 private:
  class FileSystemWatcher;

  // HoldingSpaceKeyedServiceDelegate:
  void Init() override;
  void OnHoldingSpaceItemAdded(const HoldingSpaceItem* item) override;
  void OnHoldingSpaceItemRemoved(const HoldingSpaceItem* item) override;

  // Invoked when the specified `file_path` has changed.
  void OnFilePathChanged(const base::FilePath& file_path, bool error);

  // Adds/removes a watch for the specified `file_path`.
  void AddWatch(const base::FilePath& file_path);
  void RemoveWatch(const base::FilePath& file_path);

  // Callback to invoke when file removal is detected.
  FileRemovedCallback file_removed_callback_;

  // The `file_system_watcher_` is tasked with watching the file system for
  // changes on behalf of the delegate. It does so on a non-UI sequence. As
  // such, all communication with `file_system_watcher_` must be posted via the
  // `file_system_watcher_runner_`. In return, the `file_system_watcher_` will
  // post its responses back onto the UI thread.
  std::unique_ptr<FileSystemWatcher> file_system_watcher_;
  scoped_refptr<base::SequencedTaskRunner> file_system_watcher_runner_;

  base::WeakPtrFactory<HoldingSpaceFileSystemDelegate> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_FILE_SYSTEM_DELEGATE_H_
