// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TAB_CONTEXT_TAB_CONTEXT_SYNC_SERVICE_IMPL_H_
#define COMPONENTS_SYNC_TAB_CONTEXT_TAB_CONTEXT_SYNC_SERVICE_IMPL_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/weak_ptr.h"
#include "components/sync_tab_context/container_id.h"
#include "components/sync_tab_context/tab_context_sync_service.h"

namespace syncer {
class DataTypeControllerDelegate;
class DataTypeLocalChangeProcessor;
}  // namespace syncer

namespace sync_tab_context {

class TabContextContainerSyncBridge;

class TabContextSyncServiceImpl : public TabContextSyncService {
 public:
  explicit TabContextSyncServiceImpl(
      std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor);
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
  base::WeakPtr<syncer::DataTypeControllerDelegate> GetSyncControllerDelegate()
      override;

 private:
  std::unique_ptr<TabContextContainerSyncBridge> container_bridge_;
};

}  // namespace sync_tab_context

#endif  // COMPONENTS_SYNC_TAB_CONTEXT_TAB_CONTEXT_SYNC_SERVICE_IMPL_H_
