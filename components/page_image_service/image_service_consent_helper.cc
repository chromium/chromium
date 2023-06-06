// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_image_service/image_service_consent_helper.h"

#include "base/feature_list.h"
#include "components/page_image_service/features.h"
#include "components/sync/service/sync_service.h"
#include "components/unified_consent/consent_throttle.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"

namespace page_image_service {

namespace {

constexpr base::TimeDelta kTimeout = base::Seconds(10);

}  // namespace

ImageServiceConsentHelper::ImageServiceConsentHelper(
    syncer::SyncService* sync_service,
    syncer::ModelType model_type)
    : sync_service_(sync_service), model_type_(model_type) {
  if (base::FeatureList::IsEnabled(kImageServiceObserveSyncDownloadStatus)) {
    sync_service_observer_.Observe(sync_service);
  } else if (model_type == syncer::ModelType::BOOKMARKS) {
    consent_throttle_ = std::make_unique<unified_consent::ConsentThrottle>(
        unified_consent::UrlKeyedDataCollectionConsentHelper::
            NewPersonalizedBookmarksDataCollectionConsentHelper(sync_service),
        kTimeout);
  } else if (model_type == syncer::ModelType::HISTORY_DELETE_DIRECTIVES) {
    consent_throttle_ = std::make_unique<unified_consent::ConsentThrottle>(
        unified_consent::UrlKeyedDataCollectionConsentHelper::
            NewPersonalizedDataCollectionConsentHelper(sync_service),
        kTimeout);
  } else {
    NOTREACHED();
  }
}

ImageServiceConsentHelper::~ImageServiceConsentHelper() = default;

void ImageServiceConsentHelper::EnqueueRequest(
    base::OnceCallback<void(bool)> callback) {
  if (consent_throttle_) {
    consent_throttle_->EnqueueRequest(std::move(callback));
    return;
  }

  absl::optional<bool> consent_status = GetConsentStatus();
  if (consent_status.has_value()) {
    std::move(callback).Run(*consent_status);
    return;
  }

  enqueued_request_callbacks_.emplace_back(std::move(callback));
  if (!request_processing_timer_.IsRunning()) {
    request_processing_timer_.Start(
        FROM_HERE, kTimeout,
        base::BindOnce(
            &ImageServiceConsentHelper::OnTimeoutExpired,
            // Unretained usage here okay, because this object owns the timer.
            base::Unretained(this)));
  }
}

void ImageServiceConsentHelper::OnStateChanged(
    syncer::SyncService* sync_service) {
  CHECK_EQ(sync_service_, sync_service);
  CHECK(base::FeatureList::IsEnabled(kImageServiceObserveSyncDownloadStatus));

  absl::optional<bool> consent_status = GetConsentStatus();
  if (!consent_status.has_value()) {
    return;
  }

  for (auto& request_callback : enqueued_request_callbacks_) {
    std::move(request_callback).Run(*consent_status);
  }

  enqueued_request_callbacks_.clear();
  request_processing_timer_.Stop();
}

void ImageServiceConsentHelper::OnSyncShutdown(
    syncer::SyncService* sync_service) {
  CHECK_EQ(sync_service_, sync_service);
  CHECK(base::FeatureList::IsEnabled(kImageServiceObserveSyncDownloadStatus));

  sync_service_observer_.Reset();
}

absl::optional<bool> ImageServiceConsentHelper::GetConsentStatus() {
  CHECK(base::FeatureList::IsEnabled(kImageServiceObserveSyncDownloadStatus));

  syncer::SyncService::ModelTypeDownloadStatus download_status =
      sync_service_->GetDownloadStatusFor(model_type_);
  switch (download_status) {
    case syncer::SyncService::ModelTypeDownloadStatus::kWaitingForUpdates:
      return absl::nullopt;
    case syncer::SyncService::ModelTypeDownloadStatus::kUpToDate:
      return true;
    case syncer::SyncService::ModelTypeDownloadStatus::kError:
      return false;
  }
}

void ImageServiceConsentHelper::OnTimeoutExpired() {
  for (auto& request_callback : enqueued_request_callbacks_) {
    std::move(request_callback).Run(false);
  }
  enqueued_request_callbacks_.clear();
}

}  // namespace page_image_service
