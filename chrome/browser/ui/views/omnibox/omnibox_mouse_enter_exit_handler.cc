// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_mouse_enter_exit_handler.h"

#include "ui/events/event.h"
#include "ui/views/view.h"

OmniboxMouseEnterExitHandler::OmniboxMouseEnterExitHandler(
    base::RepeatingClosure enter_exit_callback)
    : enter_exit_callback_(enter_exit_callback) {}

OmniboxMouseEnterExitHandler::~OmniboxMouseEnterExitHandler() {
  for (views::View* view : observed_views_)
    view->RemovePreTargetHandler(this);
}

void OmniboxMouseEnterExitHandler::ObserveMouseEnterExitOn(views::View* view) {
  view->AddPreTargetHandler(this);
  observed_views_.push_back(view);
}

void OmniboxMouseEnterExitHandler::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() == ui::EventType::kMouseEntered ||
      event->type() == ui::EventType::kMouseExited) {
    enter_exit_callback_.Run();
  }
}
