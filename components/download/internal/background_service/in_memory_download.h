// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_IN_MEMORY_DOWNLOAD_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_IN_MEMORY_DOWNLOAD_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "components/download/internal/background_service/blob_task_proxy.h"
#include "components/download/public/background_service/blob_context_getter_factory.h"
#include "components/download/public/background_service/download_params.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/simple_url_loader_stream_consumer.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"

class GURL;

namespace net {
struct NetworkTrafficAnnotationTag;
}  // namespace net

namespace storage {
class BlobDataHandle;
}  // namespace storage

namespace download {

struct RequestParams;

// Class to start a single download and hold in-memory download data.
// Used by download service in Incognito mode, where download files shouldn't
// be persisted to disk.
//
// Life cycle: The object is created before sending the network request.
// Call Start() to retrieve the blob storage context and send the network
// request.
class InMemoryDownload {
 public:
  class Delegate {
   public:
    // Report download progress with in-memory download backend.
    virtual void OnDownloadStarted(InMemoryDownload* download) = 0;
    virtual void OnDownloadProgress(InMemoryDownload* download) = 0;
    virtual void OnDownloadComplete(InMemoryDownload* download) = 0;
    virtual void OnUploadProgress(InMemoryDownload* download) = 0;

    // Retrieves the blob storage context getter.
    virtual void RetrieveBlobContextGetter(
        BlobContextGetterCallback callback) = 0;

   protected:
    virtual ~Delegate() = default;
  };

  // Factory to create in memory download.
  class Factory {
   public:
    virtual std::unique_ptr<InMemoryDownload> Create(
        const std::string& guid,
        const RequestParams& request_params,
        scoped_refptr<network::ResourceRequestBody> request_body,
        const net::NetworkTrafficAnnotationTag& traffic_annotation,
        Delegate* delegate) = 0;

    virtual ~Factory() = default;
  };

  // States of the download.
  enum class State {
    // The object is just created.
    INITIAL,

    // Waiting to retrieve BlobStorageContextGetter.
    RETRIEVE_BLOB_CONTEXT,

    // Download is in progress, including the following procedures.
    // 1. Send the network request and transfer data from network.
    // 2. Save the data to blob storage.
    IN_PROGRESS,

    // The download can fail due to:
    // 1. network layer failure or unsuccessful HTTP server response code.
    // 2. Blob system failures after blob construction is done.
    FAILED,

    // Download is completed, and data is successfully saved as a blob.
    // Guarantee the blob is fully constructed.
    COMPLETE,
  };

  virtual ~InMemoryDownload();

  // Send the download request.
  virtual void Start() = 0;

  // Pause the download request.
  virtual void Pause() = 0;

  // Resume the download request.
  virtual void Resume() = 0;

  // Get a copy of blob data handle.
  virtual std::unique_ptr<storage::BlobDataHandle> ResultAsBlob() const = 0;

  // Returns the estimate of dynamically allocated memory in bytes.
  virtual size_t EstimateMemoryUsage() const = 0;

  const std::string& guid() const { return guid_; }
  uint64_t bytes_downloaded() const { return bytes_downloaded_; }
  State state() const { return state_; }
  bool paused() const { return paused_; }
  const base::Time& completion_time() const { return completion_time_; }
  const std::vector<GURL>& url_chain() const { return url_chain_; }
  scoped_refptr<const net::HttpResponseHeaders> response_headers() const {
    return response_headers_;
  }
  uint64_t bytes_uploaded() const { return bytes_uploaded_; }

 protected:
  InMemoryDownload(const std::string& guid);

  // GUID of the download.
  const std::string guid_;

  State state_;

  // If the download is paused.
  bool paused_;

  // Completion time of download when data is saved as blob.
  base::Time completion_time_;

  // The URL request chain of this download.
  std::vector<GURL> url_chain_;

  // HTTP response headers.
  scoped_refptr<const net::HttpResponseHeaders> response_headers_;

  uint64_t bytes_downloaded_;

  uint64_t bytes_uploaded_;

 private:
  DISALLOW_COPY_AND_ASSIGN(InMemoryDownload);
};

// Implementation of InMemoryDownload and uses SimpleURLLoader as network
// backend.
// Threading contract:
// 1. This object lives on the main thread.
// 2. Reading/writing IO buffer from network is done on another thread,
// based on |request_context_getter_|. When complete, main thread is notified.
// 3. After network IO is done, Blob related work is done on IO thread with
// |blob_task_proxy_|, then notify the result to main thread.
class InMemoryDownloadImpl : public network::SimpleURLLoaderStreamConsumer,
                             public InMemoryDownload {
 public:
  InMemoryDownloadImpl(
      const std::string& guid,
      const RequestParams& request_params,
      scoped_refptr<network::ResourceRequestBody> request_body,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      Delegate* delegate,
      network::mojom::URLLoaderFactory* url_loader_factory,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);

  ~InMemoryDownloadImpl() override;

 private:
  // InMemoryDownload implementation.
  void Start() override;
  void Pause() override;
  void Resume() override;

  // Called when the BlobStorageContextGetter is ready to use.
  void OnRetrievedBlobContextGetter(BlobContextGetter blob_context_getter);

  std::unique_ptr<storage::BlobDataHandle> ResultAsBlob() const override;
  size_t EstimateMemoryUsage() const override;

  // network::SimpleURLLoaderStreamConsumer implementation.
  void OnDataReceived(base::StringPiece string_piece,
                      base::OnceClosure resume) override;
  void OnComplete(bool success) override;
  void OnRetry(base::OnceClosure start_retry) override;

  // Saves the download data into blob storage.
  void SaveAsBlob();
  void OnSaveBlobDone(std::unique_ptr<storage::BlobDataHandle> blob_handle,
                      storage::BlobStatus status);

  // Notifies the delegate about completion. Can be called multiple times and
  // |completion_notified_| will ensure the delegate only receive one completion
  // call.
  void NotifyDelegateDownloadComplete();

  // Sends a new network request.
  void SendRequest();

  // Called when the server redirects to another URL.
  void OnRedirect(const net::RedirectInfo& redirect_info,
                  const network::mojom::URLResponseHead& response_head,
                  std::vector<std::string>* to_be_removed_headers);

  // Called when the response of the final URL is received.
  void OnResponseStarted(const GURL& final_url,
                         const network::mojom::URLResponseHead& response_head);

  void OnUploadProgress(uint64_t position, uint64_t total);

  // Resets local states.
  void Reset();

  // Request parameters of the download.
  const RequestParams request_params_;

  // The request body to upload (if any).
  scoped_refptr<network::ResourceRequestBody> request_body_;

  // Traffic annotation of the request.
  const net::NetworkTrafficAnnotationTag traffic_annotation_;

  // Used to send requests to servers.
  std::unique_ptr<network::SimpleURLLoader> loader_;

  // Used to handle network response.
  network::mojom::URLLoaderFactory* url_loader_factory_;

  // Worker that does blob related task on IO thread.
  std::unique_ptr<BlobTaskProxy> blob_task_proxy_;

  // Owned blob data handle, so that blob system keeps at least one reference
  // count of the underlying data.
  std::unique_ptr<storage::BlobDataHandle> blob_data_handle_;

  // Used to access blob storage context.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  Delegate* delegate_;

  // Data downloaded from network, should be moved to avoid extra copy.
  std::string data_;

  // Cached callback to let network backend continue to pull data.
  base::OnceClosure resume_callback_;

  // Ensures Delegate::OnDownloadComplete is only called once.
  bool completion_notified_;

  // If |OnResponseStarted| is called.
  bool started_;

  // Bounded to main thread task runner.
  base::WeakPtrFactory<InMemoryDownloadImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(InMemoryDownloadImpl);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_IN_MEMORY_DOWNLOAD_H_
