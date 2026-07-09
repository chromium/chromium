// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TAB_CONTEXT_TAB_CONTEXT_SYNC_SERVICE_H_
#define COMPONENTS_SYNC_TAB_CONTEXT_TAB_CONTEXT_SYNC_SERVICE_H_

#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync_tab_context/container_id.h"

namespace syncer {
class DataTypeControllerDelegate;
}  // namespace syncer

namespace sync_tab_context {

// Service that allows sync-ing page context to the server in encrypted form.
// The design is based on two primitives:
// 1. Containers: a set of entries that are logically grouped and share
//    encryption keys.
// 2. Entries: individual data units that can be created, mutated or deleted.
class TabContextSyncService : public KeyedService {
 public:
  TabContextSyncService();
  ~TabContextSyncService() override;

  TabContextSyncService(const TabContextSyncService&) = delete;
  TabContextSyncService& operator=(const TabContextSyncService&) = delete;

  // Creates a new container that can later be used to upload entries into.
  // Returns nullopt in case of failure.
  virtual std::optional<ContainerId> CreateContainer() = 0;

  // Inserts or updates an entry into a container identified via
  // `container_id`. `entry_id` acts as uniqueness key within the container.
  virtual bool UploadPageContext(const ContainerId& container_id,
                                 const std::string& entry_id,
                                 std::string page_context) = 0;

  // Asynchronously returns an access token that can be used to access the
  // contents of a container. The precise means the token may be consumed is
  // unspecified and implementation-specific.
  virtual void GetContainerAccessToken(
      const ContainerId& container_id,
      base::OnceCallback<void(std::optional<std::string>)> cb) = 0;

  // Returns the delegate responsible for integrating with sync.
  virtual base::WeakPtr<syncer::DataTypeControllerDelegate>
  GetSyncControllerDelegate() = 0;
};

}  // namespace sync_tab_context

#endif  // COMPONENTS_SYNC_TAB_CONTEXT_TAB_CONTEXT_SYNC_SERVICE_H_
