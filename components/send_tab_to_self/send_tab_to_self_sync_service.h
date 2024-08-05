// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_SYNC_SERVICE_H_
#define COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_SYNC_SERVICE_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/send_tab_to_self/entry_point_display_reason.h"
#include "components/sync/model/data_type_store_service.h"
#include "components/sync/service/sync_service_observer.h"
#include "components/version_info/channel.h"

class GURL;
class PrefService;

namespace history {
class HistoryService;
}  // namespace history

namespace syncer {
class DeviceInfoTracker;
class DataTypeControllerDelegate;
class SyncService;
}  // namespace syncer

namespace send_tab_to_self {
class SendTabToSelfBridge;
class SendTabToSelfModel;

// KeyedService responsible for send tab to self sync.
class SendTabToSelfSyncService : public KeyedService,
                                 public syncer::SyncServiceObserver {
 public:
  SendTabToSelfSyncService(
      version_info::Channel channel,
      syncer::OnceDataTypeStoreFactory create_store_callback,
      history::HistoryService* history_service,
      PrefService* pref_service,
      syncer::DeviceInfoTracker* device_info_tracker);

  SendTabToSelfSyncService(const SendTabToSelfSyncService&) = delete;
  SendTabToSelfSyncService& operator=(const SendTabToSelfSyncService&) = delete;

  ~SendTabToSelfSyncService() override;

  // Hooks the cyclic dependency.
  void OnSyncServiceInitialized(syncer::SyncService* sync_service);

  // See EntryPointDisplayReason definition. Virtual for testing.
  virtual std::optional<EntryPointDisplayReason> GetEntryPointDisplayReason(
      const GURL& url_to_share);

  // Never returns null.
  virtual SendTabToSelfModel* GetSendTabToSelfModel();

  virtual base::WeakPtr<syncer::DataTypeControllerDelegate>
  GetControllerDelegate();

 protected:
  // Default constructor for unit tests.
  SendTabToSelfSyncService();

 private:
  // SyncServiceObserver implementation.
  void OnSyncShutdown(syncer::SyncService* sync_service) override;

  std::unique_ptr<SendTabToSelfBridge> const bridge_;
  raw_ptr<PrefService> const pref_service_;

  // Cyclic dependency, initialized in OnSyncServiceInitialized(), reset in
  // OnSyncShutdown().
  raw_ptr<syncer::SyncService> sync_service_ = nullptr;
};

}  // namespace send_tab_to_self

#endif  // COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_SYNC_SERVICE_H_
