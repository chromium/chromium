// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_sharing/tab_sharing_test_utils.h"

#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"

std::optional<std::u16string_view> GetTabSharingButtonOrLabelText(
    const views::View& button_or_label) {
  std::string_view class_name = button_or_label.GetClassName();
  if (class_name == "MdTextButton") {
    return static_cast<const views::MdTextButton&>(button_or_label).GetText();
  }
  if (class_name == "Label") {
    return static_cast<const views::Label&>(button_or_label).GetText();
  }
  return std::nullopt;
}
