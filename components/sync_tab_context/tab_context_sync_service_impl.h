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
#include "components/sync_tab_context/ephemeral_key_fetcher.h"
#include "components/sync_tab_context/tab_context_sync_service.h"

namespace syncer {
class DataTypeControllerDelegate;
}  // namespace syncer

namespace sync_tab_context {

class TabContextItemSyncBridge;
class TabContextContainerSyncBridge;

class TabContextSyncServiceImpl : public TabContextSyncService {
 public:
  TabContextSyncServiceImpl(
      syncer::OnceDataTypeStoreFactory store_factory,
      std::unique_ptr<EphemeralKeyFetcher> ephemeral_key_fetcher,
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
  bool IsActiveForTesting() const override;

 private:
  // Called asynchronously when `ephemeral_key_fetcher_` finishes fetching an
  // ephemeral key. Encrypts the container's key set and runs `cb`.
  void OnEphemeralKeyFetched(
      const ContainerId& container_id,
      base::OnceCallback<void(std::optional<std::string>)> cb,
      std::optional<EphemeralKeyFetcher::Result> result);

  const std::unique_ptr<TabContextContainerSyncBridge> container_bridge_;
  const std::unique_ptr<TabContextItemSyncBridge> item_bridge_;
  const std::unique_ptr<EphemeralKeyFetcher> ephemeral_key_fetcher_;

  base::WeakPtrFactory<TabContextSyncServiceImpl> weak_ptr_factory_{this};
};

}  // namespace sync_tab_context

#endif  // COMPONENTS_SYNC_TAB_CONTEXT_TAB_CONTEXT_SYNC_SERVICE_IMPL_H_
