// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/help_bubble/help_bubble.h"

#include "base/notreached.h"
#include "ui/base/interaction/element_tracker.h"

namespace user_education {

HelpBubble::ScopedNotifyOnClosed::ScopedNotifyOnClosed() = default;
HelpBubble::ScopedNotifyOnClosed::ScopedNotifyOnClosed(
    const HelpBubble* bubble,
    CloseReason reason,
    std::unique_ptr<ClosedCallbackList> callbacks)
    : reason_(reason), callbacks_(std::move(callbacks)) {}
HelpBubble::ScopedNotifyOnClosed::ScopedNotifyOnClosed(
    ScopedNotifyOnClosed&&) noexcept = default;
HelpBubble::ScopedNotifyOnClosed& HelpBubble::ScopedNotifyOnClosed::operator=(
    ScopedNotifyOnClosed&&) noexcept = default;
HelpBubble::ScopedNotifyOnClosed::~ScopedNotifyOnClosed() {
  if (callbacks_) {
    std::move(*callbacks_).Notify(reason_);
  }
}

HelpBubble::HelpBubble()
    : on_closed_callbacks_(std::make_unique<ClosedCallbackList>()) {}

HelpBubble::~HelpBubble() {
  // Derived classes must call Close() in destructor lest the bubble be
  // destroyed without cleaning up the framework-specific implementation. Since
  // Close() itself depends on framework-specific logic, however, it cannot be
  // called here, as virtual functions are no longer available in the base
  // destructor.
  CHECK(is_closed());
}

HelpBubble::ScopedNotifyOnClosed HelpBubble::BeginClose(CloseReason reason) {
  auto on_closed = std::move(on_closed_callbacks_);
  on_closing_callbacks_.Notify(this, reason);
  return ScopedNotifyOnClosed(this, reason, std::move(on_closed));
}

void HelpBubble::OnAnchorBoundsChanged() {}

gfx::Rect HelpBubble::GetBoundsInScreen() const {
  return gfx::Rect();
}

base::CallbackListSubscription HelpBubble::AddOnClosingCallback(
    ClosingCallback callback) {
  if (is_closed()) {
    NOTREACHED();
  }

  return on_closing_callbacks_.Add(std::move(callback));
}

base::CallbackListSubscription HelpBubble::AddOnClosedCallback(
    ClosedCallback callback) {
  if (is_closed()) {
    NOTREACHED();
  }

  return on_closed_callbacks_->Add(std::move(callback));
}

}  // namespace user_education
