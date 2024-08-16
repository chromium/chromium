// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/time/default_clock.h"
#include "components/history/core/browser/history_service.h"
#include "components/send_tab_to_self/features.h"
#include "components/send_tab_to_self/send_tab_to_self_bridge.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync_device_info/device_info_tracker.h"

namespace send_tab_to_self {

SendTabToSelfSyncService::SendTabToSelfSyncService() : pref_service_(nullptr) {}

SendTabToSelfSyncService::SendTabToSelfSyncService(
    version_info::Channel channel,
    syncer::OnceDataTypeStoreFactory create_store_callback,
    history::HistoryService* history_service,
    PrefService* pref_service,
    syncer::DeviceInfoTracker* device_info_tracker)
    : bridge_(std::make_unique<send_tab_to_self::SendTabToSelfBridge>(
          std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
              syncer::SEND_TAB_TO_SELF,
              base::BindRepeating(&syncer::ReportUnrecoverableError, channel)),
          base::DefaultClock::GetInstance(),
          std::move(create_store_callback),
          history_service,
          device_info_tracker)),
      pref_service_(pref_service) {}

SendTabToSelfSyncService::~SendTabToSelfSyncService() = default;

void SendTabToSelfSyncService::OnSyncServiceInitialized(
    syncer::SyncService* sync_service) {
  sync_service_ = sync_service;
  sync_service_->AddObserver(this);
}

std::optional<EntryPointDisplayReason>
SendTabToSelfSyncService::GetEntryPointDisplayReason(const GURL& url_to_share) {
  // `sync_service_` can be null in any of these cases. In all of them the
  // handling is correct because sync is not available (Yet? Anymore?).
  //   a) OnSyncServiceInitialized() didn't get called yet.
  //   b) OnSyncShutdown() already got called.
  //   c) This is a test that didn't fake the SyncService.
  //   d) Sync got disabled by command-line flag.
  // `bridge_` might be null for fake subclasses that invoked the default
  // constructor.
  return internal::GetEntryPointDisplayReason(url_to_share, sync_service_,
                                              bridge_.get(), pref_service_);
}

SendTabToSelfModel* SendTabToSelfSyncService::GetSendTabToSelfModel() {
  return bridge_.get();
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
SendTabToSelfSyncService::GetControllerDelegate() {
  return bridge_->change_processor()->GetControllerDelegate();
}

void SendTabToSelfSyncService::OnSyncShutdown(syncer::SyncService*) {
  sync_service_->RemoveObserver(this);
  sync_service_ = nullptr;
}

}  // namespace send_tab_to_self
