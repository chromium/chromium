// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/omnibox_text_field.h"

namespace vr {

OmniboxTextField::OmniboxTextField(
    float font_height_meters,
    OnInputEditedCallback input_edit_callback,
    base::RepeatingCallback<void(const AutocompleteRequest&)>
        autocomplete_start_callback,
    base::RepeatingCallback<void()> autocomplete_stop_callback)
    : TextInput(font_height_meters, input_edit_callback),
      autocomplete_start_callback_(autocomplete_start_callback),
      autocomplete_stop_callback_(autocomplete_stop_callback) {}

OmniboxTextField::~OmniboxTextField() {}

void OmniboxTextField::SetEnabled(bool enabled) {
  if (!enabled)
    autocomplete_stop_callback_.Run();
}

void OmniboxTextField::SetAutocompletion(const Autocompletion& autocompletion) {
  if (autocompletion.suffix.empty())
    return;

  TextInputInfo current = edited_text().current;
  base::string16 current_base = current.text.substr(0, current.selection_end);
  if (current_base != autocompletion.input)
    return;

  TextInputInfo info;
  info.text = current_base + autocompletion.suffix;

  // Select the autocompletion suffix, with the selection end at the previous
  // cursor position (inverted selection) so that
  // what the user typed remains in view.
  info.selection_end = current_base.size();
  info.selection_start = info.text.size();

  EditedText new_state(edited_text());
  new_state.Update(info);
  UpdateInput(new_state);
}

void OmniboxTextField::OnUpdateInput(const EditedText& info) {
  if (info.current.SelectionSize() > 0)
    return;

  AutocompleteRequest request;
  request.text = info.current.text;
  request.cursor_position = info.current.selection_end;
  request.prevent_inline_autocomplete = false;

  if (!allow_inline_autocomplete_)
    request.prevent_inline_autocomplete = true;

  // If we have a non-leading cursor position, disable autocomplete. Note that
  // the autocomplete request does take in cursor position as a input.
  if (info.current.selection_end != static_cast<int>(info.current.text.size()))
    request.prevent_inline_autocomplete = true;

  size_t previous_base_size = info.previous.text.size();
  if (info.previous.selection_end != info.previous.selection_start) {
    previous_base_size =
        std::min(info.previous.selection_start, info.previous.selection_end);
  }

  // If the new text is not larger than the previous base text, disable
  // autocomplete, as the user backspaced or removed a selection.
  if (info.current.text.size() <= previous_base_size)
    request.prevent_inline_autocomplete = true;

  autocomplete_start_callback_.Run(request);
}

}  // namespace vr
