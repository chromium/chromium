// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/content_input_delegate.h"

#include <utility>

#include "base/time/time.h"
#include "chrome/browser/vr/platform_input_handler.h"

namespace vr {

ContentInputDelegate::ContentInputDelegate() {}

ContentInputDelegate::ContentInputDelegate(PlatformInputHandler* input_handler)
    : PlatformUiInputDelegate(input_handler) {}

ContentInputDelegate::~ContentInputDelegate() = default;

void ContentInputDelegate::OnFocusChanged(bool focused) {
  // The call below tells the renderer to clear the focused element. Note that
  // we don't need to do anything when focused is true because the renderer
  // already knows about the focused element.
  if (!focused)
    input_handler()->ClearFocusedElement();
}

void ContentInputDelegate::OnWebInputEdited(const EditedText& info,
                                            bool commit) {
  if (!input_handler())
    return;

  last_keyboard_edit_ = info;

  if (commit) {
    input_handler()->SubmitWebInput();
    return;
  }

  input_handler()->OnWebInputEdited(info.GetDiff());
}

void ContentInputDelegate::OnSwapContents(int new_content_id) {
  content_id_ = new_content_id;
}

void ContentInputDelegate::SendGestureToTarget(
    std::unique_ptr<InputEvent> event) {
  if (!event || !input_handler() || ContentGestureIsLocked(event->type()))
    return;

  input_handler()->ForwardEventToContent(std::move(event), content_id_);
}

bool ContentInputDelegate::ContentGestureIsLocked(InputEvent::Type type) {
  // TODO (asimjour) create a new HoverEnter event when we swap webcontents and
  // pointer is on the content quad.
  if (type == InputEvent::kScrollBegin || type == InputEvent::kHoverMove ||
      type == InputEvent::kButtonDown || type == InputEvent::kHoverEnter)
    locked_content_id_ = content_id_;

  return locked_content_id_ != content_id_;
}

void ContentInputDelegate::OnWebInputIndicesChanged(
    int selection_start,
    int selection_end,
    int composition_start,
    int composition_end,
    TextInputUpdateCallback callback) {
  // The purpose of this method is to determine if we need to query content for
  // the text surrounding the currently focused web input field.

  // If the changed indices match with that from the last keyboard edit, then
  // this is called in response to the user entering text using the keyboard, so
  // we already know the text and don't need to ask content for it.
  TextInputInfo i = last_keyboard_edit_.current;
  if (i.selection_start == selection_start &&
      i.selection_end == selection_end &&
      i.composition_start == composition_start &&
      i.composition_end == composition_end) {
    std::move(callback).Run(i);
    return;
  }

  // Otherwise, queue up the callback
  update_state_callbacks_.emplace(std::move(callback));

  // If there's no current request, create one
  if (pending_text_request_state_ == kNoPendingRequest) {
    TextInputInfo pending_text_input_info;
    pending_text_input_info.selection_start = selection_start;
    pending_text_input_info.selection_end = selection_end;
    pending_text_input_info.composition_start = composition_start;
    pending_text_input_info.composition_end = composition_end;
    input_handler()->RequestWebInputText(base::BindOnce(
        &ContentInputDelegate::OnWebInputTextChanged, base::Unretained(this),
        std::move(pending_text_input_info)));
    pending_text_request_state_ = kRequested;
  }
}

void ContentInputDelegate::ClearTextInputState() {
  pending_text_request_state_ = kNoPendingRequest;
  last_keyboard_edit_ = EditedText();
}

void ContentInputDelegate::OnWebInputTextChanged(
    TextInputInfo pending_input_info,
    const base::string16& text) {
  pending_input_info.text = text;
  DCHECK(!update_state_callbacks_.empty());

  while (!update_state_callbacks_.empty()) {
    auto update_state_callback = std::move(update_state_callbacks_.front());
    update_state_callbacks_.pop();
    std::move(update_state_callback).Run(pending_input_info);
  }

  pending_text_request_state_ = kNoPendingRequest;
}

}  // namespace vr
