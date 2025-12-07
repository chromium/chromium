// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/batch_upload_promo/batch_upload_promo_handler.h"

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/numerics/safe_conversions.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_service.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "components/sync/base/features.h"
#include "components/sync/service/sync_service.h"

namespace {

void RunCallbackOnLocalDataDescriptionReceived(
    BatchUploadPromoHandler::GetBatchUploadPromoLocalDataCountCallback callback,
    std::map<syncer::DataType, syncer::LocalDataDescription> local_data_map) {
  int32_t local_data_count = std::accumulate(
      local_data_map.begin(), local_data_map.end(), 0,
      [](int32_t current_count,
         std::pair<syncer::DataType, syncer::LocalDataDescription> local_data) {
        return current_count + base::checked_cast<int32_t>(
                                   local_data.second.local_data_models.size());
      });

  std::move(callback).Run(local_data_count);
}

}  // namespace

BatchUploadPromoHandler::BatchUploadPromoHandler(
    mojo::PendingReceiver<batch_upload_promo::mojom::PageHandler>
        pending_page_handler,
    mojo::PendingRemote<batch_upload_promo::mojom::Page> pending_page,
    Profile* profile,
    content::WebContents* web_contents)
    : receiver_(this, std::move(pending_page_handler)),
      page_(std::move(pending_page)),
      profile_(CHECK_DEREF(profile)),
      batch_upload_service_(CHECK_DEREF(
          BatchUploadServiceFactory::GetForProfile(&profile_.get()))),
      web_contents_(web_contents) {
  sync_service_observation_.Observe(
      SyncServiceFactory::GetForProfile(&profile_.get()));
}

BatchUploadPromoHandler::~BatchUploadPromoHandler() {
  sync_service_observation_.Reset();
}

void BatchUploadPromoHandler::OnLocalDataCountChanged(
    int32_t local_data_count) {
  page_->OnLocalDataCountChanged(local_data_count);
}

void BatchUploadPromoHandler::OnBatchUploadDialogClosed() {
  GetBatchUploadPromoLocalDataCount(
      base::BindOnce(&BatchUploadPromoHandler::OnLocalDataCountChanged,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BatchUploadPromoHandler::OnStateChanged(syncer::SyncService* sync) {
  // Do not fire while the sync service is configuring, else there would be too
  // many updates.
  switch (sync->GetTransportState()) {
    case syncer::SyncService::TransportState::START_DEFERRED:
    case syncer::SyncService::TransportState::INITIALIZING:
    case syncer::SyncService::TransportState::PENDING_DESIRED_CONFIGURATION:
    case syncer::SyncService::TransportState::CONFIGURING:
      return;
    case syncer::SyncService::TransportState::DISABLED:
    case syncer::SyncService::TransportState::PAUSED:
    case syncer::SyncService::TransportState::ACTIVE:
      break;
  }

  GetBatchUploadPromoLocalDataCount(
      base::BindOnce(&BatchUploadPromoHandler::OnLocalDataCountChanged,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BatchUploadPromoHandler::OnSyncShutdown(syncer::SyncService* sync) {
  sync_service_observation_.Reset();
}

void BatchUploadPromoHandler::GetBatchUploadPromoLocalDataCount(
    GetBatchUploadPromoLocalDataCountCallback callback) {
  CHECK(base::FeatureList::IsEnabled(syncer::kUnoPhase2FollowUp));

  if (!SyncServiceFactory::IsSyncAllowed(&profile_.get())) {
    std::move(callback).Run(0);
    return;
  }

  batch_upload_service_->GetLocalDataDescriptionsForAvailableTypes(
      base::BindOnce(&RunCallbackOnLocalDataDescriptionReceived,
                     std::move(callback)));
}

void BatchUploadPromoHandler::OnBatchUploadPromoClicked() {
  CHECK(base::FeatureList::IsEnabled(syncer::kUnoPhase2FollowUp));

  Browser* browser = chrome::FindBrowserWithTab(web_contents_);
  if (!browser) {
    return;
  }

  batch_upload_service_->OpenBatchUpload(
      browser, BatchUploadService::EntryPoint::kAccountSettingsPage,
      /*dialog_shown_callback=*/base::DoNothing(),
      /*dialog_closed_callback=*/
      base::BindOnce(&BatchUploadPromoHandler::OnBatchUploadDialogClosed,
                     weak_ptr_factory_.GetWeakPtr()));
}
