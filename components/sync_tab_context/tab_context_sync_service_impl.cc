// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_tab_context/tab_context_sync_service_impl.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/notimplemented.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync_tab_context/tab_context_container_sync_bridge.h"

namespace sync_tab_context {

TabContextSyncServiceImpl::TabContextSyncServiceImpl(
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor)
    : container_bridge_(std::make_unique<TabContextContainerSyncBridge>(
          std::move(change_processor))) {}

TabContextSyncServiceImpl::~TabContextSyncServiceImpl() = default;

std::optional<ContainerId> TabContextSyncServiceImpl::CreateContainer() {
  NOTIMPLEMENTED();
  return std::nullopt;
}

bool TabContextSyncServiceImpl::UploadPageContext(
    const ContainerId& container_id,
    const std::string& entry_id,
    std::string page_context) {
  NOTIMPLEMENTED();
  return false;
}

void TabContextSyncServiceImpl::GetContainerAccessToken(
    const ContainerId& container_id,
    base::OnceCallback<void(std::optional<std::string>)> cb) {
  NOTIMPLEMENTED();
  std::move(cb).Run(std::nullopt);
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
TabContextSyncServiceImpl::GetSyncControllerDelegate() {
  return container_bridge_->change_processor()->GetControllerDelegate();
}

}  // namespace sync_tab_context
