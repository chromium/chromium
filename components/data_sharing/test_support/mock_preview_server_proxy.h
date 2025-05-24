// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_TEST_SUPPORT_MOCK_PREVIEW_SERVER_PROXY_H_
#define COMPONENTS_DATA_SHARING_TEST_SUPPORT_MOCK_PREVIEW_SERVER_PROXY_H_

#include <memory>

#include "components/data_sharing/internal/preview_server_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace data_sharing {

// The mock implementation of the PreviewServerProxy.
class MockPreviewServerProxy : public PreviewServerProxy {
 public:
  MockPreviewServerProxy();
  ~MockPreviewServerProxy() override;

  // Disallow copy/assign.
  MockPreviewServerProxy(const MockPreviewServerProxy&) = delete;
  MockPreviewServerProxy& operator=(const MockPreviewServerProxy&) = delete;

  // PreviewServerProxy Impl.
  MOCK_METHOD(
      void,
      GetSharedDataPreview,
      (const GroupToken& group_token,
       std::optional<syncer::DataType> data_type,
       base::OnceCallback<
           void(const DataSharingService::SharedDataPreviewOrFailureOutcome&)>
           callback),
      (override));
};
}  // namespace data_sharing
#endif  // COMPONENTS_DATA_SHARING_TEST_SUPPORT_MOCK_PREVIEW_SERVER_PROXY_H_
