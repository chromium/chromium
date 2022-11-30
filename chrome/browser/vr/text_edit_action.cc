// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/text_edit_action.h"

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"

namespace vr {

TextEditAction::TextEditAction(TextEditActionType type)
    : TextEditAction(type, u"", 0) {}
TextEditAction::TextEditAction(TextEditActionType type,
                               std::u16string text,
                               int new_cursor_position)
    : type_(type), text_(text), new_cursor_position_(new_cursor_position) {}

bool TextEditAction::operator==(const TextEditAction& other) const {
  return type_ == other.type() && text_ == other.text() &&
         new_cursor_position_ == other.cursor_position();
}

bool TextEditAction::operator!=(const TextEditAction& other) const {
  return !(*this == other);
}

std::string TextEditAction::ToString() const {
  return base::StringPrintf("type(%d) t(%s) c(%d)", type_,
                            base::UTF16ToUTF8(text_).c_str(),
                            new_cursor_position_);
}

}  // namespace vr
