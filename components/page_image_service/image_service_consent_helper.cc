// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_image_service/image_service_consent_helper.h"

#include "base/metrics/histogram_functions.h"
#include "components/page_image_service/metrics_util.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_utils.h"

namespace page_image_service {

namespace {

PageImageServiceConsentStatus ConsentStatusToUmaStatus(
    std::optional<bool> consent_status) {
  if (!consent_status) {
    return PageImageServiceConsentStatus::kTimedOut;
  }
  return consent_status.value() ? PageImageServiceConsentStatus::kSuccess
                                : PageImageServiceConsentStatus::kFailure;
}

}  // namespace

ImageServiceConsentHelper::ImageServiceConsentHelper(
    syncer::SyncService* sync_service,
    syncer::DataType data_type)
    : sync_service_(sync_service),
      data_type_(data_type),
      timeout_duration_(base::Seconds(10)) {
  // `sync_service` can be null, for example when disabled via flags.
  if (sync_service) {
    sync_service_observer_.Observe(sync_service);
  }
}

ImageServiceConsentHelper::~ImageServiceConsentHelper() = default;

void ImageServiceConsentHelper::EnqueueRequest(
    base::OnceCallback<void(PageImageServiceConsentStatus)> callback,
    mojom::ClientId client_id) {
  base::UmaHistogramBoolean("PageImageService.ConsentStatusRequestCount", true);

  std::optional<bool> consent_status = GetConsentStatus();
  if (consent_status.has_value()) {
    std::move(callback).Run(*consent_status
                                ? PageImageServiceConsentStatus::kSuccess
                                : PageImageServiceConsentStatus::kFailure);
    return;
  }

  enqueued_request_callbacks_.emplace_back(std::move(callback), client_id);
  if (!request_processing_timer_.IsRunning()) {
    request_processing_timer_.Start(
        FROM_HERE, timeout_duration_,
        base::BindOnce(&ImageServiceConsentHelper::OnTimeoutExpired,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void ImageServiceConsentHelper::OnStateChanged(
    syncer::SyncService* sync_service) {
  CHECK_EQ(sync_service_, sync_service);

  std::optional<bool> consent_status = GetConsentStatus();
  if (!consent_status.has_value()) {
    return;
  }

  request_processing_timer_.Stop();

  // The request callbacks can modify the vector while running. Swap the vector
  // onto the stack to prevent crashing. https://crbug.com/1472360.
  std::vector<std::pair<base::OnceCallback<void(PageImageServiceConsentStatus)>,
                        mojom::ClientId>>
      callbacks;
  std::swap(callbacks, enqueued_request_callbacks_);
  for (auto& request_callback_with_client_id : callbacks) {
    std::move(request_callback_with_client_id.first)
        .Run(*consent_status ? PageImageServiceConsentStatus::kSuccess
                             : PageImageServiceConsentStatus::kFailure);
  }
}

void ImageServiceConsentHelper::OnSyncShutdown(
    syncer::SyncService* sync_service) {
  CHECK_EQ(sync_service_, sync_service);

  sync_service_observer_.Reset();
  sync_service_ = nullptr;
}

std::optional<bool> ImageServiceConsentHelper::GetConsentStatus() {
  if (!sync_service_) {
    return false;
  }

  // If upload of the given DataType is disabled (or inactive due to an
  // error), then consent must be assumed to be NOT given.
  // Note that the "INITIALIZING" state is good enough: It means the data
  // type is enabled in principle, Sync just hasn't fully finished
  // initializing yet. This case is handled by the DownloadStatus check
  // below.
  if (syncer::GetUploadToGoogleState(sync_service_, data_type_) ==
      syncer::UploadState::NOT_ACTIVE) {
    return false;
  }

  // Ensure Sync has downloaded all relevant updates (i.e. any deletions from
  // other devices are known).
  syncer::SyncService::DataTypeDownloadStatus download_status =
      sync_service_->GetDownloadStatusFor(data_type_);
  switch (download_status) {
    case syncer::SyncService::DataTypeDownloadStatus::kWaitingForUpdates:
      return std::nullopt;
    case syncer::SyncService::DataTypeDownloadStatus::kUpToDate:
      return true;
    case syncer::SyncService::DataTypeDownloadStatus::kError:
      return false;
  }
}

void ImageServiceConsentHelper::OnTimeoutExpired() {
  // The request callbacks can modify the vector while running. Swap the vector
  // onto the stack to prevent crashing. https://crbug.com/1472360.
  std::vector<std::pair<base::OnceCallback<void(PageImageServiceConsentStatus)>,
                        mojom::ClientId>>
      callbacks;
  std::swap(callbacks, enqueued_request_callbacks_);
  for (auto& request_callback_with_client_id : callbacks) {
    // Report consent status on timeout for each request to compare against the
    // number of all requests.
    PageImageServiceConsentStatus consent_status =
        ConsentStatusToUmaStatus(GetConsentStatus());
    base::UmaHistogramEnumeration("PageImageService.ConsentStatusOnTimeout",
                                  consent_status);
    std::move(request_callback_with_client_id.first).Run(consent_status);
  }
}

}  // namespace page_image_service
