// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_INTERNAL_ANDROID_FAKE_PREVIEW_SERVER_PROXY_H_
#define COMPONENTS_DATA_SHARING_INTERNAL_ANDROID_FAKE_PREVIEW_SERVER_PROXY_H_

#include <map>

#include "components/data_sharing/internal/preview_server_proxy.h"
#include "components/data_sharing/public/group_data.h"

namespace data_sharing {

// A fake implementation of the PreviewServerProxy for testing.
class FakePreviewServerProxy : public PreviewServerProxy {
 public:
  FakePreviewServerProxy();
  ~FakePreviewServerProxy() override;

  FakePreviewServerProxy(const FakePreviewServerProxy&) = delete;
  FakePreviewServerProxy& operator=(const FakePreviewServerProxy&) = delete;

  // PreviewServerProxy impl:
  void GetSharedDataPreview(
      const GroupToken& group_token,
      std::optional<syncer::DataType> data_type,
      base::OnceCallback<
          void(const DataSharingService::SharedDataPreviewOrFailureOutcome&)>
          callback) override;

  // Sets the result of preview data for `group_id`.
  void SetSharedEntitiesPreviewForTesting(
      const GroupId& group_id,
      DataSharingService::SharedDataPreviewOrFailureOutcome preview);

 private:
  std::map<GroupId, DataSharingService::SharedDataPreviewOrFailureOutcome>
      shared_entities_preview_map_;
};

}  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_INTERNAL_ANDROID_FAKE_PREVIEW_SERVER_PROXY_H_
