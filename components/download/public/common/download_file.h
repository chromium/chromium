// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_FILE_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_FILE_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "build/build_config.h"
#include "components/download/public/common/base_file.h"
#include "components/download/public/common/download_export.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/input_stream.h"
#include "components/services/quarantine/public/mojom/quarantine.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/system/data_pipe.h"

class GURL;

namespace download {

// These objects live exclusively on the download sequence and handle the
// writing operations for one download. These objects live only for the duration
// that the download is 'in progress': once the download has been completed or
// cancelled, the DownloadFile is destroyed.
class COMPONENTS_DOWNLOAD_EXPORT DownloadFile {
 public:
  // Callback used with Initialize.
  //
  // On a successful initialize, |reason| = DOWNLOAD_INTERRUPT_REASON_NONE;
  // on a failed initialize, it will be set to the reason for the failure.
  //
  // In the case that the originally downloaded file had to be deleted,
  // |bytes_wasted| would be set to > 0.
  //
  // TODO(b/73967242): Change this to a OnceCallback. This is currently a
  // repeating callback because gMock does not support all built in actions for
  // move-only arguments (specifically SaveArg from download_item_impl_unittest.
  using InitializeCallback =
      base::RepeatingCallback<void(DownloadInterruptReason reason,
                                   int64_t bytes_wasted)>;

  // Callback used with Rename*().  On a successful rename |reason| will be
  // DOWNLOAD_INTERRUPT_REASON_NONE and |path| the path the rename
  // was done to.  On a failed rename, |reason| will contain the
  // error.
  typedef base::OnceCallback<void(DownloadInterruptReason reason,
                                  const base::FilePath& path)>
      RenameCompletionCallback;

  // Used to drop the request, when the byte stream reader should be closed on
  // download sequence.
  typedef base::RepeatingCallback<void(int64_t offset)> CancelRequestCallback;

  virtual ~DownloadFile();

  // Upon completion, |initialize_callback| will be called on the UI
  // thread as per the comment above, passing DOWNLOAD_INTERRUPT_REASON_NONE
  // on success, or a network download interrupt reason on failure.
  virtual void Initialize(
      InitializeCallback initialize_callback,
      CancelRequestCallback cancel_request_callback,
      const DownloadItem::ReceivedSlices& received_slices) = 0;

  // Add an input stream to write into a slice of the file, used for
  // parallel download.
  virtual void AddInputStream(std::unique_ptr<InputStream> stream,
                              int64_t offset) = 0;

  // Rename the download file to |full_path|.  If that file exists
  // |full_path| will be uniquified by suffixing " (<number>)" to the
  // file name before the extension.
  virtual void RenameAndUniquify(const base::FilePath& full_path,
                                 RenameCompletionCallback callback) = 0;

  // Rename the download file to |full_path| and annotate it with
  // "Mark of the Web" information about its source.  No uniquification
  // will be performed.
  // |remote_quarantine| must be connected to an instance of the Quarantine
  // service. In the unexpected case that |remote_quarantine| is invalid, or the
  // service otherwise fails, mark-of-the-web is manually applied as a fallback.
  virtual void RenameAndAnnotate(
      const base::FilePath& full_path,
      const std::string& client_guid,
      const GURL& source_url,
      const GURL& referrer_url,
      const std::optional<url::Origin>& request_initiator,
      mojo::PendingRemote<quarantine::mojom::Quarantine> remote_quarantine,
      RenameCompletionCallback callback) = 0;

  // Detach the file so it is not deleted on destruction.
  virtual void Detach() = 0;

  // Abort the download and automatically close the file.
  virtual void Cancel() = 0;

  // Sets the potential file length. This is called when a half-open range
  // request fails or completes successfully. If the range request fails, the
  // file length should not be larger than the request's offset. If the range
  // request completes successfully, the file length can be determined by
  // the request offset and the bytes received. So |length| may not be the
  // actual file length, but it should not be smaller than it.
  virtual void SetPotentialFileLength(int64_t length) = 0;

  virtual const base::FilePath& FullPath() const = 0;
  virtual bool InProgress() const = 0;

  virtual void Pause() = 0;
  virtual void Resume() = 0;

#if BUILDFLAG(IS_ANDROID)
  // Publishes the download to public. Once completes, |callback| is called with
  // the final content URI.
  virtual void PublishDownload(RenameCompletionCallback callback) = 0;
#endif  // BUILDFLAG(IS_ANDROID)

  // Whether the file is an in-memory file.
  virtual bool IsMemoryFile();
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_FILE_H_
