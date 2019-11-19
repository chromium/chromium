// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_JOB_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_JOB_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/download/public/common/download_export.h"
#include "components/download/public/common/download_file.h"
#include "components/download/public/common/download_interrupt_reasons.h"

namespace download {

class DownloadItem;

// DownloadJob lives on UI thread and subclasses implement actual download logic
// and interact with DownloadItem.
// The base class is a friend class of DownloadItem.
class COMPONENTS_DOWNLOAD_EXPORT DownloadJob {
 public:
  // Callback to cancel the download request.
  using CancelRequestCallback =
      base::OnceCallback<void(bool /* user_cancel */)>;
  CancelRequestCallback cancel_request_callback;

  DownloadJob(DownloadItem* download_item,
              CancelRequestCallback cancel_request_callback);
  virtual ~DownloadJob();

  // Download operations.
  // TODO(qinmin): Remove the |callback| and |download_file_| parameter if
  // DownloadJob owns download file.
  void Start(DownloadFile* download_file_,
             DownloadFile::InitializeCallback callback,
             const DownloadItem::ReceivedSlices& received_slices);

  virtual void Cancel(bool user_cancel);
  virtual void Pause();
  virtual void Resume(bool resume_request);

  bool is_paused() const { return is_paused_; }

  // Returns whether the download is parallelizable. The download may not send
  // parallel requests as it can be disabled through flags.
  virtual bool IsParallelizable() const;

  // Cancel a particular request starts from |offset|, while the download is not
  // canceled. Used in parallel download.
  // TODO(xingliu): Remove this function if download job owns download file.
  virtual void CancelRequestWithOffset(int64_t offset);

  // Whether the download is save package.
  virtual bool IsSavePackageDownload() const;

 protected:
  // Callback from file thread when we initialize the DownloadFile.
  virtual void OnDownloadFileInitialized(
      DownloadFile::InitializeCallback callback,
      DownloadInterruptReason result,
      int64_t bytes_wasted);

  // Add an input stream to the download sink. Return false if we start to
  // destroy download file.
  bool AddInputStream(std::unique_ptr<InputStream> stream, int64_t offset);

  DownloadItem* download_item_;

  // Callback to cancel the download, can be null.
  CancelRequestCallback cancel_request_callback_;

 private:
  // If the download progress is paused by the user.
  bool is_paused_;

  base::WeakPtrFactory<DownloadJob> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DownloadJob);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_JOB_H_
