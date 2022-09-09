// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_MOCK_ASSISTANT_STOPPED_BUBBLE_COORDINATOR_H_
#define CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_MOCK_ASSISTANT_STOPPED_BUBBLE_COORDINATOR_H_

#include "chrome/browser/ui/autofill_assistant/password_change/assistant_stopped_bubble_coordinator.h"

#include "testing/gmock/include/gmock/gmock.h"

// Mocked AssistantStoppedBubbleCoordinator used in unit tests.
class MockAssistantStoppedBubbleCoordinator
    : public AssistantStoppedBubbleCoordinator {
 public:
  MockAssistantStoppedBubbleCoordinator();
  ~MockAssistantStoppedBubbleCoordinator() override;

  MOCK_METHOD(void, Show, (), (override));
  MOCK_METHOD(void, Hide, (), (override));
  MOCK_METHOD(void, Close, (), (override));
};

#endif  // CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_MOCK_ASSISTANT_STOPPED_BUBBLE_COORDINATOR_H_
