// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill_assistant/password_change/mock_assistant_side_panel_coordinator.h"

MockAssistantSidePanelCoordinator::MockAssistantSidePanelCoordinator() =
    default;

MockAssistantSidePanelCoordinator::~MockAssistantSidePanelCoordinator() {
  Die();
}
