// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TAB_CONTEXT_FAKE_TAB_CONTEXT_SYNC_SERVICE_H_
#define COMPONENTS_SYNC_TAB_CONTEXT_FAKE_TAB_CONTEXT_SYNC_SERVICE_H_

#include <optional>
#include <set>
#include <string>
#include <string_view>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/sync/base/data_type.h"
#include "components/sync/test/fake_data_type_controller_delegate.h"
#include "components/sync_tab_context/container_id.h"
#include "components/sync_tab_context/tab_context_sync_service.h"
#include "components/sync_tab_context/upload_outcome.h"

namespace sync_tab_context {

// Fake `TabContextSyncService` implementation for tests. Supports creating
// containers in memory, retrieving fake access tokens, and validating issued
// tokens.
class FakeTabContextSyncService : public TabContextSyncService {
 public:
  FakeTabContextSyncService();
  ~FakeTabContextSyncService() override;

  // Returns whether `token` is a valid access token for `container_id`.
  bool IsValidToken(const ContainerId& container_id,
                    std::string_view token) const;

  // Synchronous counterpart to the `TabContextSyncService` overload below,
  // provided for test convenience.
  std::optional<std::string> GetContainerAccessToken(
      const ContainerId& container_id) const;

  // TabContextSyncService implementation.
  std::optional<ContainerId> CreateContainer() override;
  std::optional<ContainerId> CreateContainerPossiblyUnencryptedEphemeral()
      override;
  void UploadPageContext(
      const ContainerId& container_id,
      const std::string& entry_id,
      std::string page_context,
      base::OnceCallback<void(UploadOutcome)> callback) override;
  void GetContainerAccessToken(
      const ContainerId& container_id,
      base::OnceCallback<void(std::optional<std::string>)> cb) override;
  base::WeakPtr<syncer::DataTypeControllerDelegate>
  GetSyncControllerDelegateForContainer() override;
  base::WeakPtr<syncer::DataTypeControllerDelegate>
  GetSyncControllerDelegateForItem() override;
  bool IsActiveForTesting() const override;

 private:
  syncer::FakeDataTypeControllerDelegate container_delegate_{
      syncer::ENCRYPTED_TAB_CONTEXT_CONTAINER};
  syncer::FakeDataTypeControllerDelegate item_delegate_{
      syncer::ENCRYPTED_TAB_CONTEXT_ITEM};
  std::set<ContainerId> containers_;
};

}  // namespace sync_tab_context

#endif  // COMPONENTS_SYNC_TAB_CONTEXT_FAKE_TAB_CONTEXT_SYNC_SERVICE_H_
