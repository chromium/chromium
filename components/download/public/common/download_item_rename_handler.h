// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_ITEM_RENAME_HANDLER_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_ITEM_RENAME_HANDLER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/download/public/common/download_interrupt_reasons.h"

namespace base {
class FilePath;
}

namespace download {

class DownloadItem;

// Interface implemented by an object that overrides the default method
// of renaming a file to its final name once the download completes.
// DownloadItemImpl attempts to retrieve the object from its delegate, and
// if valid will call the Start() method instead of using
// DownloadFile::RenameAndAnnotate().
//
// Instances of DownloadItemRenameHandler are owned by DownloadItem.
class COMPONENTS_DOWNLOAD_EXPORT DownloadItemRenameHandler {
 public:
  using ProgressCallback = base::RepeatingCallback<void(int64_t bytes_so_far,
                                                        int64_t bytes_per_sec)>;
  using RenameCallback = base::OnceCallback<void(DownloadInterruptReason reason,
                                                 const base::FilePath& path)>;

  explicit DownloadItemRenameHandler(DownloadItem* download_item);
  virtual ~DownloadItemRenameHandler();

  DownloadItem* download_item() { return download_item_; }

  // Starts the process of renaming the file, invokes |progress_callback| with
  // renaming progress and invokes |rename_callback| when done.
  virtual void Start(ProgressCallback progress_callback,
                     RenameCallback rename_callback);

  // Returns whether there's rename progress to be shown.
  virtual bool ShowRenameProgress();

 protected:
  raw_ptr<DownloadItem> download_item_;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_ITEM_RENAME_HANDLER_H_
