// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_MOCK_ASSISTANT_SIDE_PANEL_COORDINATOR_H_
#define CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_MOCK_ASSISTANT_SIDE_PANEL_COORDINATOR_H_

#include "chrome/browser/ui/autofill_assistant/password_change/assistant_side_panel_coordinator.h"

#include <memory>

#include "testing/gmock/include/gmock/gmock.h"
#include "ui/views/view.h"

class MockAssistantSidePanelCoordinator : public AssistantSidePanelCoordinator {
 public:
  MockAssistantSidePanelCoordinator();
  ~MockAssistantSidePanelCoordinator() override;
  MOCK_METHOD(void, Die, ());

  // AssistantDisplayDelegate:
  MOCK_METHOD(views::View*,
              SetView,
              (std::unique_ptr<views::View>),
              (override));
  MOCK_METHOD(views::View*, GetView, (), (override));
  MOCK_METHOD(void, RemoveView, (), (override));

  // AssistantSidePanelCoordinator:
  MOCK_METHOD(bool, Shown, (), (override));
  MOCK_METHOD(void, AddObserver, (Observer*), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer*), (override));
};

#endif  // CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_MOCK_ASSISTANT_SIDE_PANEL_COORDINATOR_H_
