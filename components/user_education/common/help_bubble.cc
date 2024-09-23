// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/help_bubble.h"

#include "base/notreached.h"
#include "ui/base/interaction/element_tracker.h"

namespace user_education {

HelpBubble::HelpBubble()
    : on_close_callbacks_(std::make_unique<CallbackList>()) {}

HelpBubble::~HelpBubble() {
  // Derived classes must call Close() in destructor lest the bubble be
  // destroyed without cleaning up the framework-specific implementation. Since
  // Close() itself depends on framework-specific logic, however, it cannot be
  // called here, as virtual functions are no longer available in the base
  // destructor.
  CHECK(is_closed());
}

bool HelpBubble::Close(CloseReason close_reason) {
  // This prevents us from re-entrancy during CloseBubbleImpl() or after the
  // bubble is closed.
  if (is_closed()) {
    return false;
  }

  // We can't destruct the callback list during callbacks, so ensure that it
  // sticks around until the callbacks are all finished. This also has the side
  // effect of making is_closed() true since it resets the value of
  // `on_close_callbacks_`.
  std::unique_ptr<CallbackList> callbacks = std::move(on_close_callbacks_);

  // Note: any of the following could destroy `this`.

  // Actually close the help bubble. For some implementations, this may trigger
  // additional events.
  CloseBubbleImpl();

  // Call any on-close callbacks.
  if (callbacks) {
    callbacks->Notify(this, close_reason);
  }

  return true;
}

void HelpBubble::OnAnchorBoundsChanged() {}

gfx::Rect HelpBubble::GetBoundsInScreen() const {
  return gfx::Rect();
}

base::CallbackListSubscription HelpBubble::AddOnCloseCallback(
    ClosedCallback callback) {
  if (is_closed()) {
    NOTREACHED_IN_MIGRATION();
    return base::CallbackListSubscription();
  }

  return on_close_callbacks_->Add(std::move(callback));
}

}  // namespace user_education
