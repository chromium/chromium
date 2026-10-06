// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TAB_CONTEXT_MOCK_TAB_CONTEXT_SYNC_SERVICE_H_
#define COMPONENTS_SYNC_TAB_CONTEXT_MOCK_TAB_CONTEXT_SYNC_SERVICE_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/sync/model/data_type_controller_delegate.h"
#include "components/sync_tab_context/container_id.h"
#include "components/sync_tab_context/tab_context_sync_service.h"
#include "components/sync_tab_context/upload_outcome.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace sync_tab_context {

class MockTabContextSyncService : public TabContextSyncService {
 public:
  MockTabContextSyncService();
  ~MockTabContextSyncService() override;

  MOCK_METHOD(std::optional<ContainerId>, CreateContainer, (), (override));
  MOCK_METHOD(void,
              UploadPageContext,
              (const ContainerId&,
               const std::string&,
               std::string,
               base::OnceCallback<void(UploadOutcome)>),
              (override));
  MOCK_METHOD(void,
              GetContainerAccessToken,
              (const ContainerId&,
               base::OnceCallback<void(std::optional<std::string>)>),
              (override));
  MOCK_METHOD(base::WeakPtr<syncer::DataTypeControllerDelegate>,
              GetSyncControllerDelegateForContainer,
              (),
              (override));
  MOCK_METHOD(base::WeakPtr<syncer::DataTypeControllerDelegate>,
              GetSyncControllerDelegateForItem,
              (),
              (override));
  MOCK_METHOD(bool, IsActiveForTesting, (), (const, override));
};

}  // namespace sync_tab_context

#endif  // COMPONENTS_SYNC_TAB_CONTEXT_MOCK_TAB_CONTEXT_SYNC_SERVICE_H_
