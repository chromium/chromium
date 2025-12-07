// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/internal/android/fake_preview_server_proxy.h"

#include "base/types/expected.h"
#include "components/data_sharing/internal/preview_server_proxy.h"
#include "components/data_sharing/public/data_sharing_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace data_sharing {

FakePreviewServerProxy::FakePreviewServerProxy()
    : PreviewServerProxy(nullptr, nullptr, version_info::Channel::STABLE) {}

FakePreviewServerProxy::~FakePreviewServerProxy() = default;

void FakePreviewServerProxy::GetSharedDataPreview(
    const GroupToken& group_token,
    std::optional<syncer::DataType> data_type,
    base::OnceCallback<
        void(const DataSharingService::SharedDataPreviewOrFailureOutcome&)>
        callback) {
  if (shared_entities_preview_map_.find(group_token.group_id) !=
      shared_entities_preview_map_.end()) {
    return std::move(callback).Run(
        shared_entities_preview_map_[group_token.group_id]);
  }
  return std::move(callback).Run(base::unexpected(
      DataSharingService::DataPreviewActionFailure::kOtherFailure));
}

void FakePreviewServerProxy::SetSharedEntitiesPreviewForTesting(
    const GroupId& group_id,
    DataSharingService::SharedDataPreviewOrFailureOutcome preview) {
  shared_entities_preview_map_[group_id] = std::move(preview);
}

}  // namespace data_sharing
