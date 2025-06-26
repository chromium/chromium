// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_TEST_SUPPORT_MOCK_VERSIONING_MESSAGE_CONTROLLER_H_
#define COMPONENTS_SAVED_TAB_GROUPS_TEST_SUPPORT_MOCK_VERSIONING_MESSAGE_CONTROLLER_H_

#include "base/functional/callback.h"
#include "components/saved_tab_groups/public/versioning_message_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace tab_groups {

class MockVersioningMessageController : public VersioningMessageController {
 public:
  MockVersioningMessageController();
  ~MockVersioningMessageController() override;

  MOCK_METHOD(bool, IsInitialized, (), (override));
  MOCK_METHOD(bool, ShouldShowMessageUi, (MessageType), (override));
  MOCK_METHOD(void,
              ShouldShowMessageUiAsync,
              (MessageType, base::OnceCallback<void(bool)>),
              (override));
  MOCK_METHOD(void, OnMessageUiShown, (MessageType), (override));
  MOCK_METHOD(void, OnMessageUiDismissed, (MessageType), (override));
};

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_TEST_SUPPORT_MOCK_VERSIONING_MESSAGE_CONTROLLER_H_
