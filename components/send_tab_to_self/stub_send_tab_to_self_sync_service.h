// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEND_TAB_TO_SELF_STUB_SEND_TAB_TO_SELF_SYNC_SERVICE_H_
#define COMPONENTS_SEND_TAB_TO_SELF_STUB_SEND_TAB_TO_SELF_SYNC_SERVICE_H_

#include <memory>
#include <optional>

#include "base/memory/weak_ptr.h"
#include "components/send_tab_to_self/entry_point_display_reason.h"
#include "components/send_tab_to_self/fake_send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/sync/test/fake_data_type_controller_delegate.h"

class GURL;

namespace send_tab_to_self {

class StubSendTabToSelfSyncService : public SendTabToSelfSyncService {
 public:
  StubSendTabToSelfSyncService();

  StubSendTabToSelfSyncService(const StubSendTabToSelfSyncService&) = delete;
  StubSendTabToSelfSyncService& operator=(const StubSendTabToSelfSyncService&) =
      delete;

  ~StubSendTabToSelfSyncService() override;

  // SendTabToSelfSyncService:
  std::optional<EntryPointDisplayReason> GetEntryPointDisplayReason(
      const GURL& url_to_share) override;
  SendTabToSelfModel* GetSendTabToSelfModel() override;
  base::WeakPtr<syncer::DataTypeControllerDelegate> GetControllerDelegate()
      override;

  void SetEntryPointDisplayReason(
      std::optional<EntryPointDisplayReason> reason);

  FakeSendTabToSelfModel* GetFakeSendTabToSelfModel();

 private:
  FakeSendTabToSelfModel model_;
  syncer::FakeDataTypeControllerDelegate fake_delegate_;
  std::optional<EntryPointDisplayReason> entry_point_display_reason_;
};

}  // namespace send_tab_to_self

#endif  // COMPONENTS_SEND_TAB_TO_SELF_STUB_SEND_TAB_TO_SELF_SYNC_SERVICE_H_
