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

  MOCK_METHOD(void,
              PrepareFlowUI,
              (base::OnceCallback<void()> exit_callback, ResultCallback result),
              (override));
  MOCK_METHOD(void,
              ShowError,
              (const ErrorInfo& error, ResultCallback result),
              (override));
  MOCK_METHOD(void, Cancel, (ResultCallback result), (override));
  MOCK_METHOD(void,
              ShowAuthenticationUi,
              (FlowType flow_type, ResultCallback result),
              (override));
  MOCK_METHOD(void, NotifySignInAndSyncStatusChange, (), (override));
  MOCK_METHOD(void,
              ShowJoinDialog,
              (const data_sharing::GroupToken& token,
               const data_sharing::SharedDataPreview& preview_data,
               ResultCallback result),
              (override));
  MOCK_METHOD(
      void,
      ShowShareDialog,
      (const tab_groups::EitherGroupID& either_id,
       base::OnceCallback<
           void(Outcome, std::optional<data_sharing::GroupToken>)> result),
      (override));
  MOCK_METHOD(void,
              OnUrlReadyToShare,
              (const data_sharing::GroupId& group_id,
               const GURL& url,
               ResultCallback result),
              (override));
  MOCK_METHOD(void,
              ShowManageDialog,
              (const tab_groups::EitherGroupID& either_id,
               ResultCallback result),
              (override));
  MOCK_METHOD(void,
              ShowLeaveDialog,
              (const tab_groups::EitherGroupID& either_id,
               ResultCallback result),
              (override));
  MOCK_METHOD(void,
              ShowDeleteDialog,
              (const tab_groups::EitherGroupID& either_id,
               ResultCallback result),
              (override));
  MOCK_METHOD(void,
              PromoteTabGroup,
              (const data_sharing::GroupId& group_id, ResultCallback result),
              (override));
  MOCK_METHOD(void, PromoteCurrentScreen, (), (override));
  MOCK_METHOD(void, OnFlowFinished, (), (override));
#if BUILDFLAG(IS_ANDROID)
  MOCK_METHOD(base::android::ScopedJavaLocalRef<jobject>,
              GetJavaObject,
              (),
              (override));
#endif  // BUILDFLAG(IS_ANDROID)
};

}  // namespace collaboration

#endif  // COMPONENTS_COLLABORATION_TEST_SUPPORT_MOCK_COLLABORATION_CONTROLLER_DELEGATE_H_
