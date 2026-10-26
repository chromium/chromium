// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/dictation/text_field_filter.h"

#include "content/public/browser/render_frame_host.h"

namespace dictation {

TextFieldSize GetTextFieldSize(
    content::EditableLevel editable_level,
    std::optional<ui::TextInputType> text_input_type) {
  // TODO(b:576375438): Use more detailed heuristics, e.g. the element's size,
  // since short form fields can also be textareas or contenteditables.
  switch (editable_level) {
    case content::EditableLevel::kNotEditable:
      return TextFieldSize::kNotEditable;
    case content::EditableLevel::kRichlyEditable:
      // contenteditable elements, e.g. email and document editors.
      return TextFieldSize::kMediumOrLarge;
    case content::EditableLevel::kPlaintextEditable:
      if (!text_input_type) {
        return TextFieldSize::kUnknown;
      }
      // Textareas and plaintext-only contenteditables, but not one-line inputs.
      if (*text_input_type == ui::TEXT_INPUT_TYPE_TEXT_AREA ||
          *text_input_type == ui::TEXT_INPUT_TYPE_CONTENT_EDITABLE) {
        return TextFieldSize::kMediumOrLarge;
      }
      return TextFieldSize::kSmall;
  }
}

TextFieldSize GetFocusedElementTextFieldSize(content::RenderFrameHost& rfh) {
  return GetTextFieldSize(rfh.GetFocusedEditableLevel(),
                          rfh.GetFocusedElementTextInputType());
}

}  // namespace dictation
