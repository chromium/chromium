// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/dragging/tab_drag_controller_interactive_steps_mixin.h"

#include "ui/base/interaction/polling_state_observer.h"
#include "ui/base/interaction/state_observer.h"

DEFINE_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<size_t>,
                              kTabDragBrowserCountState);
DEFINE_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<bool>,
                              kTabDragActiveState);
DEFINE_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<bool>,
                              kTabDragWindowLoopActiveState);
DEFINE_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<bool>,
                              kTabDragAttachedToTargetState);
DEFINE_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<bool>,
                              kTabDragNewBrowserActiveState);
