// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_ITEM_RENAME_HANDLER_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_ITEM_RENAME_HANDLER_H_

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/download/public/common/download_item_rename_progress_update.h"

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
  // Callback to update the DownloadItem and send some info into databases.
  using ProgressUpdateCallback = base::RepeatingCallback<void(
      const download::DownloadItemRenameProgressUpdate&)>;
  // Callback when the rename handler processing completes. Args indicate
  // processing result to be updated to UX, and the final file name validated at
  // rerouted location.
  using DownloadCallback =
      base::OnceCallback<void(DownloadInterruptReason, const base::FilePath&)>;

  explicit DownloadItemRenameHandler(DownloadItem* download_item);
  virtual ~DownloadItemRenameHandler();

  DownloadItem* download_item() { return download_item_; }

  // Starts the process of renaming the file and invokes |callback| when
  // done.
  virtual void Start(ProgressUpdateCallback progress_update_cb,
                     DownloadCallback upload_complete_cb) = 0;

  // Opens the file associated with this download.
  virtual void OpenDownload() = 0;

  // Shows the download in the context of its container.
  virtual void ShowDownloadInContext() = 0;

 private:
  raw_ptr<DownloadItem> download_item_;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_ITEM_RENAME_HANDLER_H_
