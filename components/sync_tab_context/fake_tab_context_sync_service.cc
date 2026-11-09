// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_tab_context/fake_tab_context_sync_service.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/notreached.h"
#include "base/uuid.h"

namespace sync_tab_context {

namespace {
constexpr char kFakeTokenPrefix[] = "fake_decryption_token_";
}  // namespace

FakeTabContextSyncService::FakeTabContextSyncService() = default;

FakeTabContextSyncService::~FakeTabContextSyncService() = default;

bool FakeTabContextSyncService::IsValidToken(const ContainerId& container_id,
                                             std::string_view token) const {
  return GetContainerAccessToken(container_id) == token;
}

std::optional<std::string> FakeTabContextSyncService::GetContainerAccessToken(
    const ContainerId& container_id) const {
  if (!containers_.contains(container_id)) {
    return std::nullopt;
  }
  return kFakeTokenPrefix + container_id.value().AsLowercaseString();
}

std::optional<ContainerId> FakeTabContextSyncService::CreateContainer() {
  ContainerId container_id(base::Uuid::GenerateRandomV4());
  containers_.insert(container_id);
  return container_id;
}

std::optional<ContainerId>
FakeTabContextSyncService::CreateContainerPossiblyUnencryptedEphemeral() {
  return CreateContainer();
}

void FakeTabContextSyncService::UploadPageContext(
    const ContainerId& container_id,
    const std::string& entry_id,
    std::string page_context,
    base::OnceCallback<void(UploadOutcome)> callback) {
  NOTREACHED();
}

void FakeTabContextSyncService::GetContainerAccessToken(
    const ContainerId& container_id,
    base::OnceCallback<void(std::optional<std::string>)> cb) {
  std::move(cb).Run(GetContainerAccessToken(container_id));
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
FakeTabContextSyncService::GetSyncControllerDelegateForContainer() {
  return container_delegate_.GetWeakPtr();
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
FakeTabContextSyncService::GetSyncControllerDelegateForItem() {
  return item_delegate_.GetWeakPtr();
}

bool FakeTabContextSyncService::IsActiveForTesting() const {
  NOTREACHED();
}

}  // namespace sync_tab_context
