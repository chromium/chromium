// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_COMMON_PARALLEL_DOWNLOAD_JOB_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_COMMON_PARALLEL_DOWNLOAD_JOB_H_

#include <memory>
#include <unordered_map>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "components/download/internal/common/download_job_impl.h"
#include "components/download/internal/common/download_worker.h"
#include "components/download/public/common/download_create_info.h"
#include "components/download/public/common/download_export.h"
#include "components/download/public/common/parallel_download_configs.h"
#include "components/download/public/common/url_loader_factory_provider.h"

namespace service_manager {
class Connector;
}  // namespace service_manager

namespace download {

// DownloadJob that can create concurrent range requests to fetch different
// parts of the file.
// The original request is hold in base class.
class COMPONENTS_DOWNLOAD_EXPORT ParallelDownloadJob
    : public DownloadJobImpl,
      public DownloadWorker::Delegate {
 public:
  // TODO(qinmin): Remove |url_request_context_getter| once network service is
  // enabled.
  ParallelDownloadJob(DownloadItem* download_item,
                      CancelRequestCallback cancel_request_callback,
                      const DownloadCreateInfo& create_info,
                      URLLoaderFactoryProvider::URLLoaderFactoryProviderPtr
                          url_loader_factory_provider,
                      service_manager::Connector* connector);
  ~ParallelDownloadJob() override;

  // DownloadJobImpl implementation.
  void Cancel(bool user_cancel) override;
  void Pause() override;
  void Resume(bool resume_request) override;
  void CancelRequestWithOffset(int64_t offset) override;

 protected:
  // DownloadJobImpl implementation.
  void OnDownloadFileInitialized(DownloadFile::InitializeCallback callback,
                                 DownloadInterruptReason result,
                                 int64_t bytes_wasted) override;

  // Virtual for testing.
  virtual int GetParallelRequestCount() const;
  virtual int64_t GetMinSliceSize() const;
  virtual int GetMinRemainingTimeInSeconds() const;

  using WorkerMap =
      std::unordered_map<int64_t, std::unique_ptr<DownloadWorker>>;

  // Map from the offset position of the slice to the worker that downloads the
  // slice.
  WorkerMap workers_;

 private:
  friend class ParallelDownloadJobTest;

  // DownloadWorker::Delegate implementation.
  void OnInputStreamReady(
      DownloadWorker* worker,
      std::unique_ptr<InputStream> input_stream,
      std::unique_ptr<DownloadCreateInfo> download_create_info) override;

  // Build parallel requests after a delay, to effectively measure the single
  // stream bandwidth.
  void BuildParallelRequestAfterDelay();

  // Build parallel requests to download. This function is the entry point for
  // all parallel downloads.
  void BuildParallelRequests();

  // Build one http request for each slice from the second slice.
  // The first slice represents the original request.
  void ForkSubRequests(const DownloadItem::ReceivedSlices& slices_to_download);

  // Create one range request, virtual for testing. Range request will start
  // from |offset| and will be half open.
  virtual void CreateRequest(int64_t offset);

  // Information about the initial request when download is started.
  int64_t initial_request_offset_;

  // A snapshot of received slices when creating the parallel download job.
  // Download item's received slices may be different from this snapshot when
  // |BuildParallelRequests| is called.
  DownloadItem::ReceivedSlices initial_received_slices_;

  // The length of the response body of the original request.
  // Used to estimate the remaining size of the content when the initial
  // request is half open, i.e, |initial_request_length_| is
  // DownloadSaveInfo::kLengthFullContent.
  int64_t content_length_;

  // Used to send parallel requests after a delay based on Finch config.
  base::OneShotTimer timer_;

  // If we have sent parallel requests.
  bool requests_sent_;

  // If the download progress is canceled.
  bool is_canceled_;

  // Whether the server accepts range requests.
  RangeRequestSupportType range_support_;

  // URLLoaderFactoryProvider to retrieve the URLLoaderFactory and issue
  // parallel requests.
  URLLoaderFactoryProvider::URLLoaderFactoryProviderPtr
      url_loader_factory_provider_;

  // Connector used for establishing the connection to the ServiceManager.
  service_manager::Connector* connector_;

  DISALLOW_COPY_AND_ASSIGN(ParallelDownloadJob);
};

}  //  namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_COMMON_PARALLEL_DOWNLOAD_JOB_H_
