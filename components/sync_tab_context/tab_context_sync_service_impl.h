// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TAB_CONTEXT_TAB_CONTEXT_SYNC_SERVICE_IMPL_H_
#define COMPONENTS_SYNC_TAB_CONTEXT_TAB_CONTEXT_SYNC_SERVICE_IMPL_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync_tab_context/container_id.h"
#include "components/sync_tab_context/tab_context_sync_service.h"

namespace syncer {
class DataTypeControllerDelegate;
}  // namespace syncer

namespace sync_tab_context {

class TabContextItemSyncBridge;
class TabContextContainerSyncBridge;

class TabContextSyncServiceImpl : public TabContextSyncService {
 public:
  TabContextSyncServiceImpl(syncer::OnceDataTypeStoreFactory store_factory,
                            base::RepeatingClosure dump_stack);
  TabContextSyncServiceImpl(const TabContextSyncServiceImpl&) = delete;
  TabContextSyncServiceImpl& operator=(const TabContextSyncServiceImpl&) =
      delete;
  ~TabContextSyncServiceImpl() override;

  // TabContextSyncService implementation.
  std::optional<ContainerId> CreateContainer() override;
  bool UploadPageContext(const ContainerId& container_id,
                         const std::string& entry_id,
                         std::string page_context) override;
  void GetContainerAccessToken(
      const ContainerId& container_id,
      base::OnceCallback<void(std::optional<std::string>)> cb) override;
  base::WeakPtr<syncer::DataTypeControllerDelegate>
  GetSyncControllerDelegateForContainer() override;
  base::WeakPtr<syncer::DataTypeControllerDelegate>
  GetSyncControllerDelegateForItem() override;

 private:
  std::unique_ptr<TabContextContainerSyncBridge> container_bridge_;
  std::unique_ptr<TabContextItemSyncBridge> item_bridge_;
};

}  // namespace sync_tab_context

#endif  // COMPONENTS_SYNC_TAB_CONTEXT_TAB_CONTEXT_SYNC_SERVICE_IMPL_H_
