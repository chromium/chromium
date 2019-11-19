// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_SYNC_SERVICE_H_
#define COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_SYNC_SERVICE_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/model/model_type_store_service.h"
#include "components/version_info/channel.h"

namespace history {
class HistoryService;
}  // namespace history

namespace syncer {
class DeviceInfoTracker;
class ModelTypeControllerDelegate;
}  // namespace syncer

namespace send_tab_to_self {
class SendTabToSelfBridge;
class SendTabToSelfModel;

// KeyedService responsible for send tab to self sync.
class SendTabToSelfSyncService : public KeyedService {
 public:
  SendTabToSelfSyncService(
      version_info::Channel channel,
      syncer::OnceModelTypeStoreFactory create_store_callback,
      history::HistoryService* history_service,
      syncer::DeviceInfoTracker* device_info_tracker);
  ~SendTabToSelfSyncService() override;

  virtual SendTabToSelfModel* GetSendTabToSelfModel();

  // For ProfileSyncService to initialize the controller.
  virtual base::WeakPtr<syncer::ModelTypeControllerDelegate>
  GetControllerDelegate();

 protected:
  // Default constructor for unit tests
  SendTabToSelfSyncService();

 private:
  std::unique_ptr<SendTabToSelfBridge> bridge_;

  DISALLOW_COPY_AND_ASSIGN(SendTabToSelfSyncService);
};

}  // namespace send_tab_to_self

#endif  // COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_SYNC_SERVICE_H_
