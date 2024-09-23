// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/common/parallel_download_job.h"

#include <algorithm>

#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/time/time.h"
#include "components/download/internal/common/parallel_download_utils.h"
#include "components/download/public/common/download_create_info.h"
#include "components/download/public/common/download_stats.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/referrer_policy.h"

namespace download {
namespace {
const int kDownloadJobVerboseLevel = 1;
}  // namespace

ParallelDownloadJob::ParallelDownloadJob(
    DownloadItem* download_item,
    CancelRequestCallback cancel_request_callback,
    const DownloadCreateInfo& create_info,
    URLLoaderFactoryProvider::URLLoaderFactoryProviderPtr
        url_loader_factory_provider,
    DownloadJobFactory::WakeLockProviderBinder wake_lock_provider_binder)
    : DownloadJobImpl(download_item, std::move(cancel_request_callback), true),
      initial_request_offset_(create_info.offset),
      initial_received_slices_(download_item->GetReceivedSlices()),
      content_length_(create_info.total_bytes),
      requests_sent_(false),
      is_canceled_(false),
      url_loader_factory_provider_(std::move(url_loader_factory_provider)),
      wake_lock_provider_binder_(std::move(wake_lock_provider_binder)) {}

ParallelDownloadJob::~ParallelDownloadJob() = default;

void ParallelDownloadJob::OnDownloadFileInitialized(
    DownloadFile::InitializeCallback callback,
    DownloadInterruptReason result,
    int64_t bytes_wasted) {
  DownloadJobImpl::OnDownloadFileInitialized(std::move(callback), result,
                                             bytes_wasted);
  if (result == DOWNLOAD_INTERRUPT_REASON_NONE)
    BuildParallelRequestAfterDelay();
}

void ParallelDownloadJob::Cancel(bool user_cancel) {
  is_canceled_ = true;
  DownloadJobImpl::Cancel(user_cancel);

  if (!requests_sent_) {
    timer_.Stop();
    return;
  }

  for (auto& worker : workers_)
    worker.second->Cancel(user_cancel);
}

void ParallelDownloadJob::Pause() {
  DownloadJobImpl::Pause();

  if (!requests_sent_) {
    timer_.Stop();
    return;
  }

  for (auto& worker : workers_)
    worker.second->Pause();
}

void ParallelDownloadJob::Resume(bool resume_request) {
  DownloadJobImpl::Resume(resume_request);
  if (!resume_request)
    return;

  // Send parallel requests if the download is paused previously.
  if (!requests_sent_) {
    if (!timer_.IsRunning())
      BuildParallelRequestAfterDelay();
    return;
  }

  for (auto& worker : workers_)
    worker.second->Resume();
}

int ParallelDownloadJob::GetParallelRequestCount() const {
  return GetParallelRequestCountConfig();
}

int64_t ParallelDownloadJob::GetMinSliceSize() const {
  return GetMinSliceSizeConfig();
}

int ParallelDownloadJob::GetMinRemainingTimeInSeconds() const {
  return GetParallelRequestRemainingTimeConfig().InSeconds();
}

void ParallelDownloadJob::CancelRequestWithOffset(int64_t offset) {
  if (initial_request_offset_ == offset) {
    DownloadJobImpl::Cancel(false);
    return;
  }

  auto it = workers_.find(offset);
  CHECK(it != workers_.end(), base::NotFatalUntil::M130);
  it->second->Cancel(false);
}

void ParallelDownloadJob::BuildParallelRequestAfterDelay() {
  DCHECK(workers_.empty());
  DCHECK(!requests_sent_);
  DCHECK(!timer_.IsRunning());

  timer_.Start(FROM_HERE, GetParallelRequestDelayConfig(), this,
               &ParallelDownloadJob::BuildParallelRequests);
}

void ParallelDownloadJob::OnInputStreamReady(
    DownloadWorker* worker,
    std::unique_ptr<InputStream> input_stream,
    std::unique_ptr<DownloadCreateInfo> download_create_info) {
  bool success =
      DownloadJob::AddInputStream(std::move(input_stream), worker->offset());

  // Destroy the request if the sink is gone.
  if (!success) {
    VLOG(kDownloadJobVerboseLevel)
        << "Byte stream arrived after download file is released.";
    worker->Cancel(false);
  }
}

void ParallelDownloadJob::BuildParallelRequests() {
  DCHECK(!requests_sent_);
  DCHECK(!is_paused());
  if (is_canceled_ ||
      download_item_->GetState() != DownloadItem::DownloadState::IN_PROGRESS) {
    return;
  }

  // TODO(qinmin): The size of |slices_to_download| should be no larger than
  // |kParallelRequestCount| unless |kParallelRequestCount| is changed after
  // a download is interrupted. This could happen if we use finch to config
  // the number of parallel requests.
  // Get the next |kParallelRequestCount - 1| slices and fork
  // new requests. For the remaining slices, they will be handled once some
  // of the workers finish their job.
  const DownloadItem::ReceivedSlices& received_slices =
      download_item_->GetReceivedSlices();
  DownloadItem::ReceivedSlices slices_to_download =
      FindSlicesToDownload(received_slices);

  DCHECK(!slices_to_download.empty());
  int64_t first_slice_offset = slices_to_download[0].offset;

  // We may build parallel job without slices. The slices can be cleared or
  // previous session only has one stream writing to disk. In these cases, fall
  // back to non parallel download.
  if (initial_request_offset_ > first_slice_offset) {
    VLOG(kDownloadJobVerboseLevel)
        << "Received slices data mismatch initial request offset.";
    return;
  }

  // Create more slices for a new download. The initial request may generate
  // a received slice.
  if (slices_to_download.size() <= 1 && download_item_->GetTotalBytes() > 0) {
    int64_t current_bytes_per_second =
        std::max(static_cast<int64_t>(1), download_item_->CurrentSpeed());
    int64_t remaining_bytes =
        download_item_->GetTotalBytes() - download_item_->GetReceivedBytes();

    if (remaining_bytes / current_bytes_per_second >
        GetMinRemainingTimeInSeconds()) {
      // Fork more requests to accelerate, only if one slice is left to download
      // and remaining time seems to be long enough.
      slices_to_download = FindSlicesForRemainingContent(
          first_slice_offset,
          content_length_ - first_slice_offset + initial_request_offset_,
          GetParallelRequestCount(), GetMinSliceSize());
    }
  }

  DCHECK(!slices_to_download.empty());

  // If the last received slice is finished, remove the last request which can
  // be out of the range of the file. E.g, the file is 100 bytes, and the last
  // request's range header will be "Range:100-".
  if (!received_slices.empty() && received_slices.back().finished)
    slices_to_download.pop_back();

  if (slices_to_download.empty())
    return;

  ForkSubRequests(slices_to_download);

  requests_sent_ = true;
}

void ParallelDownloadJob::ForkSubRequests(
    const DownloadItem::ReceivedSlices& slices_to_download) {
  // If the initial request is working on the first hole, don't create parallel
  // request for this hole.
  bool skip_first_slice = true;
  DownloadItem::ReceivedSlices initial_slices_to_download =
      FindSlicesToDownload(initial_received_slices_);
  if (initial_slices_to_download.size() > 1) {
    DCHECK_EQ(initial_request_offset_, initial_slices_to_download[0].offset);
    int64_t first_hole_max = initial_slices_to_download[0].offset +
                             initial_slices_to_download[0].received_bytes;
    skip_first_slice = slices_to_download[0].offset <= first_hole_max;
  }

  for (auto it = slices_to_download.begin(); it != slices_to_download.end();
       ++it) {
    if (skip_first_slice) {
      skip_first_slice = false;
      continue;
    }

    DCHECK_GE(it->offset, initial_request_offset_);
    // All parallel requests are half open, which sends request headers like
    // "Range:50-".
    // If server rejects a certain request, others should take over.
    CreateRequest(it->offset);
  }
}

void ParallelDownloadJob::CreateRequest(int64_t offset) {
  DCHECK(download_item_);

  auto worker = std::make_unique<DownloadWorker>(this, offset);

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("parallel_download_job", R"(
        semantics {
          sender: "Parallel Download"
          description:
            "Chrome makes parallel request to speed up download of a file."
          trigger:
            "When user starts a download request, if it would be technically "
            "possible, Chrome starts parallel downloading."
          data: "None."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting: "This feature cannot be disabled in settings."
          chrome_policy {
            DownloadRestrictions {
              DownloadRestrictions: 3
            }
          }
        })");
  // The parallel requests only use GET method.
  std::unique_ptr<DownloadUrlParameters> download_params(
      new DownloadUrlParameters(download_item_->GetURL(), traffic_annotation));
  download_params->set_file_path(download_item_->GetFullPath());
  download_params->set_last_modified(download_item_->GetLastModifiedTime());
  download_params->set_etag(download_item_->GetETag());
  download_params->set_offset(offset);

  // Subsequent range requests don't need the "If-Range" header.
  download_params->set_use_if_range(false);

  // Subsequent range requests have the same referrer URL as the original
  // download request.
  download_params->set_referrer(download_item_->GetReferrerUrl());
  download_params->set_referrer_policy(net::ReferrerPolicy::NEVER_CLEAR);

  // TODO(xingliu): We should not support redirect at all for parallel requests.
  // Currently the network service code path still can redirect as long as it's
  // the same origin.
  download_params->set_cross_origin_redirects(
      network::mojom::RedirectMode::kError);

  // Send the request.
  mojo::PendingRemote<device::mojom::WakeLockProvider> wake_lock_provider;
  wake_lock_provider_binder_.Run(
      wake_lock_provider.InitWithNewPipeAndPassReceiver());
  worker->SendRequest(std::move(download_params),
                      url_loader_factory_provider_.get(),
                      std::move(wake_lock_provider));
  DCHECK(workers_.find(offset) == workers_.end());
  workers_[offset] = std::move(worker);
}

}  // namespace download
