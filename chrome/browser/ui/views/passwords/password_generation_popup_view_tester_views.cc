// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_generation_popup_view_tester_views.h"

#include "chrome/browser/ui/views/passwords/password_generation_popup_view_views.h"
#include "ui/events/event_utils.h"

std::unique_ptr<PasswordGenerationPopupViewTester>
PasswordGenerationPopupViewTester::For(PasswordGenerationPopupView* view) {
  return std::make_unique<PasswordGenerationPopupViewTesterViews>(
      static_cast<PasswordGenerationPopupViewViews*>(view));
}

PasswordGenerationPopupViewTesterViews::PasswordGenerationPopupViewTesterViews(
    PasswordGenerationPopupViewViews* popup_view)
    : view_(popup_view) {}

PasswordGenerationPopupViewTesterViews::
    ~PasswordGenerationPopupViewTesterViews() {}

void PasswordGenerationPopupViewTesterViews::SimulateMouseMovementAt(
    const gfx::Point& point) {
  ui::MouseEvent mouse_down(ui::ET_MOUSE_MOVED, point, gfx::Point(0, 0),
                            ui::EventTimeForNow(), 0, 0);
  static_cast<views::View*>(view_)->OnMouseMoved(mouse_down);
}

bool PasswordGenerationPopupViewTesterViews::IsPopupMinimized() const {
  return view_->IsPopupMinimized();
}
