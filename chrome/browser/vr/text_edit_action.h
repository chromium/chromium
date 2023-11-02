// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_TEXT_EDIT_ACTION_H_
#define CHROME_BROWSER_VR_TEXT_EDIT_ACTION_H_

#include <string>
#include <vector>

#include "chrome/browser/vr/vr_base_export.h"

namespace vr {

// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.vr
enum TextEditActionType {
  CLEAR_COMPOSING_TEXT,
  COMMIT_TEXT,
  SET_COMPOSING_TEXT,
  DELETE_TEXT,
};

class VR_BASE_EXPORT TextEditAction {
 public:
  explicit TextEditAction(TextEditActionType type);
  TextEditAction(TextEditActionType type,
                 std::u16string text,
                 int new_cursor_position);

  TextEditActionType type() const { return type_; }
  std::u16string text() const { return text_; }
  int cursor_position() const { return new_cursor_position_; }

  bool operator==(const TextEditAction& other) const;
  bool operator!=(const TextEditAction& other) const;

  std::string ToString() const;

 private:
  TextEditActionType type_;
  std::u16string text_;
  int new_cursor_position_;
};

typedef std::vector<TextEditAction> TextEdits;

}  // namespace vr

#endif  // CHROME_BROWSER_VR_TEXT_EDIT_ACTION_H_
