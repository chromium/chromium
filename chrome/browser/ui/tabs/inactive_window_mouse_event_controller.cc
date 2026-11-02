// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/inactive_window_mouse_event_controller.h"

#include "base/memory/weak_ptr.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace tabs {

DEFINE_USER_DATA(InactiveWindowMouseEventController);

InactiveWindowMouseEventController::InactiveWindowMouseEventController(
    TabInterface& tab)
    : scoped_unowned_user_data_(tab.GetUnownedUserDataHost(), *this) {}

InactiveWindowMouseEventController::~InactiveWindowMouseEventController() =
    default;

// static
InactiveWindowMouseEventController* InactiveWindowMouseEventController::From(
    TabInterface* tab) {
  return tab ? Get(tab->GetUnownedUserDataHost()) : nullptr;
}

ScopedAcceptMouseEventsWhileWindowInactive::
    ScopedAcceptMouseEventsWhileWindowInactive(
        base::WeakPtr<InactiveWindowMouseEventController> controller)
    : controller_(controller) {
  if (controller_) {
    controller_->Increment(InactiveWindowMouseEventController::PassKey{});
  }
}

ScopedAcceptMouseEventsWhileWindowInactive::
    ~ScopedAcceptMouseEventsWhileWindowInactive() {
  if (controller_) {
    controller_->Decrement(InactiveWindowMouseEventController::PassKey{});
  }
}

bool InactiveWindowMouseEventController::
    ShouldAcceptMouseEventsWhileWindowInactive() const {
  return accept_input_counter_ > 0;
}

std::unique_ptr<ScopedAcceptMouseEventsWhileWindowInactive>
InactiveWindowMouseEventController::AcceptMouseEventsWhileWindowInactive() {
  return std::make_unique<ScopedAcceptMouseEventsWhileWindowInactive>(
      GetWeakPtr());
}

void InactiveWindowMouseEventController::Increment(PassKey key) {
  ++accept_input_counter_;
}

void InactiveWindowMouseEventController::Decrement(PassKey key) {
  --accept_input_counter_;
}

}  // namespace tabs
