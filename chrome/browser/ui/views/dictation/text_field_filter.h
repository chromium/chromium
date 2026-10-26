// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DICTATION_TEXT_FIELD_FILTER_H_
#define CHROME_BROWSER_UI_VIEWS_DICTATION_TEXT_FIELD_FILTER_H_

#include <optional>

#include "content/public/browser/editable_level.h"
#include "ui/base/ime/text_input_type.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace dictation {

// The size of a text field, i.e. how much text users usually write in it.
enum class TextFieldSize {
  // Not an editable field.
  kNotEditable,
  // A one-line input, e.g. text, search, email or URL.
  kSmall,
  // A textarea or contenteditable element.
  kMediumOrLarge,
  // A plain text field whose text input type hasn't arrived yet.
  kUnknown,
};

// Returns the size of a field. Pass
// `content::RenderFrameHost::GetFocusedEditableLevel()` and
// `GetFocusedElementTextInputType()`.
TextFieldSize GetTextFieldSize(
    content::EditableLevel editable_level,
    std::optional<ui::TextInputType> text_input_type);

// Returns the size of the element focused in `rfh`.
TextFieldSize GetFocusedElementTextFieldSize(content::RenderFrameHost& rfh);

}  // namespace dictation

#endif  // CHROME_BROWSER_UI_VIEWS_DICTATION_TEXT_FIELD_FILTER_H_
