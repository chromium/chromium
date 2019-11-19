// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_ITEM_IMPL_DELEGATE_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_ITEM_IMPL_DELEGATE_H_

#include <stdint.h>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/optional.h"
#include "components/download/public/common/download_export.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_url_parameters.h"
#include "components/download/public/common/quarantine_connection.h"
#include "components/services/quarantine/public/mojom/quarantine.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace service_manager {
class Connector;
}  // namespace service_manager

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
  typedef base::Callback<void(bool)> ShouldOpenDownloadCallback;

  DownloadItemImplDelegate();
  virtual ~DownloadItemImplDelegate();

  // Used for catching use-after-free errors.
  void Attach();
  void Detach();

  using DownloadTargetCallback =
      base::Callback<void(const base::FilePath& target_path,
                          DownloadItem::TargetDisposition disposition,
                          DownloadDangerType danger_type,
                          const base::FilePath& intermediate_path,
                          DownloadInterruptReason interrupt_reason)>;
  // Request determination of the download target from the delegate.
  virtual void DetermineDownloadTarget(DownloadItemImpl* download,
                                       const DownloadTargetCallback& callback);

  // Allows the delegate to delay completion of the download.  This function
  // will either return true (if the download may complete now) or will return
  // false and call the provided callback at some future point.  This function
  // may be called repeatedly.
  virtual bool ShouldCompleteDownload(DownloadItemImpl* download,
                                      const base::Closure& complete_callback);

  // Allows the delegate to override the opening of a download. If it returns
  // true then it's reponsible for opening the item.
  virtual bool ShouldOpenDownload(DownloadItemImpl* download,
                                  const ShouldOpenDownloadCallback& callback);

  // Tests if a file type should be opened automatically.
  virtual bool ShouldOpenFileBasedOnExtension(const base::FilePath& path);

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
      const GURL& site_url);

  // Update the persistent store with our information.
  virtual void UpdatePersistence(DownloadItemImpl* download);

  // Opens the file associated with this download.
  virtual void OpenDownload(DownloadItemImpl* download);

  // Returns whether this is the most recent download in the rare event where
  // multiple downloads are associated with the same file path.
  virtual bool IsMostRecentDownloadItemAtFilePath(DownloadItemImpl* download);

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

  // Gets the ServiceManager connector that can be used on UI thread.
  virtual service_manager::Connector* GetServiceManagerConnector();

  // Gets a callback that can connect to the Quarantine Service if available.
  virtual QuarantineConnectionCallback GetQuarantineConnectionCallback();

 private:
  // For "Outlives attached DownloadItemImpl" invariant assertion.
  int count_;

  DISALLOW_COPY_AND_ASSIGN(DownloadItemImplDelegate);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_ITEM_IMPL_DELEGATE_H_
