// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/keyboard_delegate_for_testing.h"

#include <algorithm>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/vr/keyboard_ui_interface.h"
#include "chrome/browser/vr/model/text_input_info.h"
#include "chrome/browser/vr/ui_test_input.h"

namespace vr {

KeyboardDelegateForTesting::KeyboardDelegateForTesting() {
  cached_keyboard_input_ = TextInputInfo();
}

KeyboardDelegateForTesting::~KeyboardDelegateForTesting() = default;

void KeyboardDelegateForTesting::QueueKeyboardInputForTesting(
    KeyboardTestInput keyboard_input) {
  keyboard_input_queue_.push(keyboard_input);
}

bool KeyboardDelegateForTesting::IsQueueEmpty() const {
  return keyboard_input_queue_.empty();
}

void KeyboardDelegateForTesting::SetUiInterface(KeyboardUiInterface* ui) {
  ui_ = ui;
}

void KeyboardDelegateForTesting::ShowKeyboard() {
  keyboard_shown_ = true;
}
void KeyboardDelegateForTesting::HideKeyboard() {
  keyboard_shown_ = false;
}
void KeyboardDelegateForTesting::SetTransform(const gfx::Transform&) {}
bool KeyboardDelegateForTesting::HitTest(const gfx::Point3F& ray_origin,
                                         const gfx::Point3F& ray_target,
                                         gfx::Point3F* touch_position) {
  return false;
}

void KeyboardDelegateForTesting::OnBeginFrame() {
  if (!keyboard_shown_ || IsQueueEmpty() || pause_keyboard_input_)
    return;

  DCHECK(ui_);
  KeyboardTestInput input = keyboard_input_queue_.front();
  keyboard_input_queue_.pop();
  TextInputInfo next_info;
  auto current_string = cached_keyboard_input_.text;
  int cursor_start = std::min(cached_keyboard_input_.selection_start,
                              cached_keyboard_input_.selection_end);
  int new_selection_start;

  switch (input.action) {
    case KeyboardTestAction::kInputText:
      // We can either be inputting text at a cursor position or replacing
      // selected text.
      if (cached_keyboard_input_.SelectionSize() == 0) {
        // Inputting at cursor.
        current_string.insert(cached_keyboard_input_.selection_start,
                              base::UTF8ToUTF16(input.input_text));
      } else {
        // Replacing selected text.
        current_string.replace(cursor_start,
                               cached_keyboard_input_.SelectionSize(),
                               base::UTF8ToUTF16(input.input_text));
      }
      new_selection_start = cursor_start + input.input_text.length();
      next_info = TextInputInfo(current_string, new_selection_start,
                                new_selection_start);
      break;
    case KeyboardTestAction::kBackspace:
      // We can either be deleting at a cursor position or deleting selected
      // text.
      if (cached_keyboard_input_.SelectionSize() == 0) {
        // Deleting at cursor.
        // We can't delete if the cursor is at the start, so no-op.
        if (cursor_start == 0) {
          return;
        }
        current_string.erase(cursor_start - 1, 1);
        new_selection_start = cursor_start - 1;
      } else {
        // Deleting selected text.
        current_string.erase(cursor_start,
                             cached_keyboard_input_.SelectionSize());
        new_selection_start = cursor_start;
      }
      next_info = TextInputInfo(current_string, new_selection_start,
                                new_selection_start);
      break;
    case KeyboardTestAction::kEnter:
      ui_->OnInputCommitted(
          EditedText(cached_keyboard_input_, cached_keyboard_input_));
      return;
    default:
      NOTREACHED() << "Given unsupported controller action";
  }
  ui_->OnInputEdited(EditedText(next_info, cached_keyboard_input_));
  pause_keyboard_input_ = true;
}

void KeyboardDelegateForTesting::Draw(const CameraModel&) {}

bool KeyboardDelegateForTesting::SupportsSelection() {
  return true;
}

void KeyboardDelegateForTesting::UpdateInput(const TextInputInfo& info) {
  cached_keyboard_input_ = info;
  pause_keyboard_input_ = false;
}

}  // namespace vr
