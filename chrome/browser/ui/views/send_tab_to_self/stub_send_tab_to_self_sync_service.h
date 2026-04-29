// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_STUB_SEND_TAB_TO_SELF_SYNC_SERVICE_H_
#define CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_STUB_SEND_TAB_TO_SELF_SYNC_SERVICE_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/send_tab_to_self/fake_send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/sync/test/fake_data_type_controller_delegate.h"

namespace content {
class BrowserContext;
}

namespace send_tab_to_self {

class StubSendTabToSelfSyncService : public SendTabToSelfSyncService {
 public:
  StubSendTabToSelfSyncService();
  ~StubSendTabToSelfSyncService() override;

  SendTabToSelfModel* GetSendTabToSelfModel() override;

  base::WeakPtr<syncer::DataTypeControllerDelegate> GetControllerDelegate()
      override;

  std::optional<EntryPointDisplayReason> GetEntryPointDisplayReason(
      const GURL& url_to_share) override;

  FakeSendTabToSelfModel* GetModelFake();

 protected:
  syncer::FakeDataTypeControllerDelegate fake_delegate_;
  FakeSendTabToSelfModel model_fake_;
};

std::unique_ptr<KeyedService> BuildStubSyncService(
    content::BrowserContext* context);

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_STUB_SEND_TAB_TO_SELF_SYNC_SERVICE_H_
