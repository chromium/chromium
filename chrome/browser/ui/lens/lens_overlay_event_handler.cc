// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_overlay_event_handler.h"

#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "components/lens/lens_overlay_dismissal_source.h"

namespace lens {

namespace {
bool IsEscapeEvent(const input::NativeWebKeyboardEvent& event) {
  return event.GetType() == input::NativeWebKeyboardEvent::Type::kRawKeyDown &&
         event.windows_key_code == ui::VKEY_ESCAPE;
}

bool IsCopyEvent(const input::NativeWebKeyboardEvent& event) {
  const int key_modifiers =
      event.GetModifiers() & blink::WebInputEvent::kKeyModifiers;
  return event.windows_key_code == ui::VKEY_C &&
         (key_modifiers == blink::WebInputEvent::kControlKey ||
          key_modifiers == blink::WebInputEvent::kMetaKey);
}
}  // namespace

LensOverlayEventHandler::LensOverlayEventHandler(
    LensOverlayController* lens_overlay_controller)
    : lens_overlay_controller_(lens_overlay_controller) {}

bool LensOverlayEventHandler::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event,
    views::FocusManager* focus_manager) {
  if (!focus_manager || !lens_overlay_controller_->IsOverlayShowing()) {
    return false;
  }

  if (IsEscapeEvent(event)) {
    lens_overlay_controller_->CloseUIAsync(
        lens::LensOverlayDismissalSource::kEscapeKeyPress);
    return true;
  }
  // We only want to copy if the user is not currently making a native text
  // selection. If the user is currently making a native text selection, we
  // assume the CMD/CTRL + C event is to select that text.
  if (IsCopyEvent(event) && !source->GetFocusedFrame()->HasSelection()) {
    lens_overlay_controller_->TriggerCopyText();
    return true;
  }
  return unhandled_keyboard_event_handler_.HandleKeyboardEvent(event,
                                                               focus_manager);
}
}  // namespace lens
