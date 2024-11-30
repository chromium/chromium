// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COLLABORATION_TEST_SUPPORT_MOCK_COLLABORATION_CONTROLLER_DELEGATE_H_
#define COMPONENTS_COLLABORATION_TEST_SUPPORT_MOCK_COLLABORATION_CONTROLLER_DELEGATE_H_

#include "components/collaboration/public/collaboration_controller_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace collaboration {

class MockCollaborationControllerDelegate
    : public CollaborationControllerDelegate {
 public:
  MockCollaborationControllerDelegate();
  ~MockCollaborationControllerDelegate() override;

  MOCK_METHOD(void, PrepareFlowUI, (ResultCallback result), (override));
  MOCK_METHOD(void,
              ShowError,
              (ResultCallback result, const ErrorInfo& error),
              (override));
  MOCK_METHOD(void, Cancel, (ResultCallback result), (override));
  MOCK_METHOD(void, ShowAuthenticationUi, (ResultCallback result), (override));
  MOCK_METHOD(void, NotifySignInAndSyncStatusChange, (), (override));
  MOCK_METHOD(void,
              ShowJoinDialog,
              (data_sharing::SharedDataPreview preview_data,
               ResultCallback result),
              (override));
  MOCK_METHOD(void, ShowShareDialog, (ResultCallback result), (override));
  MOCK_METHOD(void, PromoteTabGroup, (ResultCallback result), (override));
  MOCK_METHOD(void, PromoteCurrentScreen, (), (override));
};

}  // namespace collaboration

#endif  // COMPONENTS_COLLABORATION_TEST_SUPPORT_MOCK_COLLABORATION_CONTROLLER_DELEGATE_H_
