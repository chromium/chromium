// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_ITEM_IMPL_DELEGATE_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_ITEM_IMPL_DELEGATE_H_

#include <stdint.h>

#include <memory>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "components/download/public/common/download_export.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_target_info.h"
#include "components/download/public/common/download_url_parameters.h"
#include "components/download/public/common/quarantine_connection.h"
#include "components/services/quarantine/public/mojom/quarantine.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"

namespace download {
class DownloadItemImpl;

// Delegate for operations that a DownloadItemImpl can't do for itself.
// The base implementation of this class does nothing (returning false
// on predicates) so interfaces not of interest to a derived class may
// be left unimplemented.
class COMPONENTS_DOWNLOAD_EXPORT DownloadItemImplDelegate {
 public:
  // The boolean argument indicates whether or not the download was
  // actually opened.
  using ShouldOpenDownloadCallback = base::OnceCallback<void(bool)>;

  DownloadItemImplDelegate();

  DownloadItemImplDelegate(const DownloadItemImplDelegate&) = delete;
  DownloadItemImplDelegate& operator=(const DownloadItemImplDelegate&) = delete;

  virtual ~DownloadItemImplDelegate();

  // Used for catching use-after-free errors.
  void Attach();
  void Detach();

  // Request determination of the download target from the delegate.
  virtual void DetermineDownloadTarget(DownloadItemImpl* download,
                                       DownloadTargetCallback callback);

  // Allows the delegate to delay completion of the download.  This function
  // will either return true (if the download may complete now) or will return
  // false and call the provided callback at some future point.  This function
  // may be called repeatedly.
  virtual bool ShouldCompleteDownload(DownloadItemImpl* download,
                                      base::OnceClosure complete_callback);

  // Allows the delegate to override the opening of a download. If it returns
  // true then it's responsible for opening the item, and the |callback| is not
  // run.
  virtual bool ShouldOpenDownload(DownloadItemImpl* download,
                                  ShouldOpenDownloadCallback callback);

  // Tests if a file type should be opened automatically.
  virtual bool ShouldAutomaticallyOpenFile(const GURL& url,
                                           const base::FilePath& path);

  // Tests if a file type should be opened automatically by policy.
  virtual bool ShouldAutomaticallyOpenFileByPolicy(const GURL& url,
                                                   const base::FilePath& path);

  // Checks whether a downloaded file still exists and updates the
  // file's state if the file is already removed.
  // The check may or may not result in a later asynchronous call
  // to OnDownloadedFileRemoved().
  virtual void CheckForFileRemoval(DownloadItemImpl* download_item);

  // Return a GUID string used for identifying the application to the system AV
  // function for scanning downloaded files. If no GUID is provided or if the
  // provided GUID is invalid, then the appropriate quarantining will be
  // performed manually without passing the download to the system AV function.
  //
  // This GUID is only used on Windows.
  virtual std::string GetApplicationClientIdForFileScanning() const;

  // Called when an interrupted download is resumed.
  virtual void ResumeInterruptedDownload(
      std::unique_ptr<DownloadUrlParameters> params,
      const std::string& serialized_embedder_data);

  // Update the persistent store with our information.
  virtual void UpdatePersistence(DownloadItemImpl* download);

  // Opens the file associated with this download.
  virtual void OpenDownload(DownloadItemImpl* download);

  // Shows the download via the OS shell.
  virtual void ShowDownloadInShell(DownloadItemImpl* download);

  // Handle any delegate portions of a state change operation on the
  // DownloadItem.
  virtual void DownloadRemoved(DownloadItemImpl* download);

  // Called when the download is interrupted.
  virtual void DownloadInterrupted(DownloadItemImpl* download);

  // Whether the download is off the record.
  virtual bool IsOffTheRecord() const;

  // Check if the current network is a metered network.
  virtual bool IsActiveNetworkMetered() const;

  // Report extra bytes wasted during resumption.
  virtual void ReportBytesWasted(DownloadItemImpl* download);

  // Binds a device.mojom.WakeLockProvider receiver from the UI thread.
  virtual void BindWakeLockProvider(
      mojo::PendingReceiver<device::mojom::WakeLockProvider> receiver);

  // Gets a callback that can connect to the Quarantine Service if available.
  virtual QuarantineConnectionCallback GetQuarantineConnectionCallback();

  // Gets a handler to perform the rename for a download item.  If no special
  // rename handling is required, this function returns null and the default
  // rename handling is performed.
  virtual std::unique_ptr<DownloadItemRenameHandler>
  GetRenameHandlerForDownload(DownloadItemImpl* download_item);

 private:
  // For "Outlives attached DownloadItemImpl" invariant assertion.
  int count_;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_ITEM_IMPL_DELEGATE_H_
