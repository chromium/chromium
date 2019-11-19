// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_BACKGROUND_SERVICE_CLIENT_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_BACKGROUND_SERVICE_CLIENT_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "net/http/http_response_headers.h"
#include "url/gurl.h"

namespace network {
class ResourceRequestBody;
}  // namespace network

namespace download {

struct CompletionInfo;
struct DownloadMetaData;

using GetUploadDataCallback =
    base::OnceCallback<void(scoped_refptr<network::ResourceRequestBody>)>;

// The Client interface required by any feature that wants to start a download
// through the DownloadService.  Should be registered immediately at startup
// when the DownloadService is created (see the factory).
class Client {
 public:
  // Used by OnDownloadFailed to determine the reason of the abort.
  enum class FailureReason {
    // Used when the download has been aborted after reaching a threshold where
    // we decide it is not worth attempting to start again.  This could be
    // either due to a specific number of failed retry attempts or a specific
    // number of wasted bytes due to the download restarting.
    NETWORK,

    // Used when the download was not completed before the
    // DownloadParams::cancel_after timeout.
    TIMEDOUT,

    // Used when the upload data was not received from the client before
    // timeout.
    UPLOAD_TIMEDOUT,

    // Used when the download was cancelled by the Client.
    CANCELLED,

    // Used when the download was aborted by the Client in response to the
    // download starting (see OnDownloadStarted()).
    ABORTED,

    // Used when the failure reason is unknown.  This generally means that we
    // detect that the download failed during a restart, but aren't sure exactly
    // what triggered the failure before shutdown.
    UNKNOWN,
  };

  virtual ~Client() = default;

  // Called when the DownloadService is initialized and ready to be interacted
  // with.  |downloads| is a list of all downloads the DownloadService is aware
  // of that are associated with this Client, including in-progress downloads
  // and recently completed downloads.
  // If |state_lost| is |true|, the service ran into an error initializing and
  // had to destroy all internal persisted state.  At this point any saved files
  // might not be available and any previously scheduled downloads are gone.
  virtual void OnServiceInitialized(
      bool state_lost,
      const std::vector<DownloadMetaData>& downloads) = 0;

  // Called when the DownloadService fails to initialize and should not be used.
  virtual void OnServiceUnavailable() = 0;

  // Will be called when a download has been started and the headers have been
  // retrieved. The download will be downloading at the time this call is made.
  virtual void OnDownloadStarted(
      const std::string& guid,
      const std::vector<GURL>& url_chain,
      const scoped_refptr<const net::HttpResponseHeaders>& headers);

  // Will be called when there is an update to the current progress state of the
  // underlying download.  Note that |bytes_downloaded| may go backwards if the
  // download had to be started over from the beginning due to an interruption.
  // This will be called frequently if the download is actively downloading,
  // with byte updates coming in as they are processed by the internal download
  // driver.
  virtual void OnDownloadUpdated(const std::string& guid,
                                 uint64_t bytes_uploaded,
                                 uint64_t bytes_downloaded);

  // Called when a download failed.  Check FailureReason for a list of possible
  // reasons why this failure occurred.  Note that this will also be called for
  // cancelled downloads. The CompletionInfo will be provided with the URL chain
  // and response headers filled in if available.
  virtual void OnDownloadFailed(const std::string& guid,
                                const download::CompletionInfo& info,
                                FailureReason reason);

  // Called when a download has been successfully completed.
  // The file and the database record will be automatically removed if it is not
  // renamed or deleted after a window of time (12 hours, but finch
  // configurable).
  // The timeout is meant to be a failsafe to ensure that we clean up properly.
  // TODO(dtrainor): Point to finch configurable timeout when it is added.
  virtual void OnDownloadSucceeded(const std::string& guid,
                                   const CompletionInfo& completion_info) = 0;

  // Called by the service to ask the client whether it is okay to remove a
  // completed file. If true, the file is deleted. If false, the file life time
  // is granted another grace period (12 hours, configurable) after which
  // the service will try again. If |force_delete| is true which happens when
  // the file was completed too long ago, the file will be deleted regardless of
  // the outcome of this function.
  virtual bool CanServiceRemoveDownloadedFile(const std::string& guid,
                                              bool force_delete) = 0;

  // Called by the service to ask the client to provide the upload data.
  // The client is responsible for posting the callback with an appropriate
  // ResourceRequestBody or nullptr, if it is a regular download.
  virtual void GetUploadData(const std::string& guid,
                             GetUploadDataCallback callback) = 0;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_BACKGROUND_SERVICE_CLIENT_H_
