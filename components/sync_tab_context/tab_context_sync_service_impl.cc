// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_tab_context/tab_context_sync_service_impl.h"

#include <utility>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/crypto/agile_symmetric_key_set.h"
#include "components/sync_tab_context/tab_context_container_sync_bridge.h"
#include "components/sync_tab_context/tab_context_item_sync_bridge.h"

namespace sync_tab_context {

TabContextSyncServiceImpl::TabContextSyncServiceImpl(
    base::RepeatingClosure dump_stack)
    : container_bridge_(std::make_unique<TabContextContainerSyncBridge>(
          std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
              syncer::ENCRYPTED_TAB_CONTEXT_CONTAINER,
              dump_stack))),
      item_bridge_(std::make_unique<TabContextItemSyncBridge>(
          std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
              syncer::ENCRYPTED_TAB_CONTEXT_ITEM,
              dump_stack))) {}

TabContextSyncServiceImpl::~TabContextSyncServiceImpl() = default;

std::optional<ContainerId> TabContextSyncServiceImpl::CreateContainer() {
  return container_bridge_->CreateContainer();
}

bool TabContextSyncServiceImpl::UploadPageContext(
    const ContainerId& container_id,
    const std::string& entry_id,
    std::string page_context) {
  const syncer::AgileSymmetricKeySet* key_set =
      container_bridge_->GetEncryptionKeyForContainer(container_id);
  if (!key_set) {
    return false;
  }

  // TODO(crbug.com/527991322): Consider compression before encryption, capping
  // the size and moving expensive operations to a backend sequence.
  std::optional<sync_pb::EncryptedData> encrypted_data =
      key_set->Encrypt(base::as_byte_span(page_context));
  if (!encrypted_data) {
    return false;
  }

  return item_bridge_->UploadItem(container_id, entry_id,
                                  std::move(*encrypted_data));
}

void TabContextSyncServiceImpl::GetContainerAccessToken(
    const ContainerId& container_id,
    base::OnceCallback<void(std::optional<std::string>)> cb) {
  NOTIMPLEMENTED();
  std::move(cb).Run(std::nullopt);
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
TabContextSyncServiceImpl::GetSyncControllerDelegateForContainer() {
  return container_bridge_->change_processor()->GetControllerDelegate();
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
TabContextSyncServiceImpl::GetSyncControllerDelegateForItem() {
  return item_bridge_->change_processor()->GetControllerDelegate();
}

}  // namespace sync_tab_context
