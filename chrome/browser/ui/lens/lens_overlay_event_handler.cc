// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_overlay_event_handler.h"

#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_dismissal_source.h"

namespace lens {

namespace {
bool IsEscapeEvent(const input::NativeWebKeyboardEvent& event) {
  return event.GetType() == input::NativeWebKeyboardEvent::Type::kRawKeyDown &&
         event.windows_key_code == ui::VKEY_ESCAPE;
}
}  // namespace

LensOverlayEventHandler::LensOverlayEventHandler(
    LensOverlayController* lens_overlay_controller)
    : lens_overlay_controller_(lens_overlay_controller) {}

bool LensOverlayEventHandler::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event,
    views::FocusManager* focus_manager) {
  if (!focus_manager) {
    return false;
  }

  if (IsEscapeEvent(event) && lens_overlay_controller_->IsOverlayShowing()) {
    lens_overlay_controller_->CloseUIAsync(
        lens::LensOverlayDismissalSource::kEscapeKeyPress);
    return true;
  }
  return unhandled_keyboard_event_handler_.HandleKeyboardEvent(event,
                                                               focus_manager);
}
}  // namespace lens
