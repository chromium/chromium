// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOOKMARKS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_OVERFLOW_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_BOOKMARKS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_OVERFLOW_BUTTON_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/menu_button.h"

namespace tab_groups {

// TODO(pengchaocai): Reword to "Everything" for V2.
class SavedTabGroupOverflowButton : public views::MenuButton {
  METADATA_HEADER(SavedTabGroupOverflowButton, views::MenuButton)

 public:
  explicit SavedTabGroupOverflowButton(PressedCallback callback);
  ~SavedTabGroupOverflowButton() override;

  SavedTabGroupOverflowButton(const SavedTabGroupOverflowButton&) = delete;
  SavedTabGroupOverflowButton& operator=(const SavedTabGroupOverflowButton&) =
      delete;

  std::unique_ptr<views::LabelButtonBorder> CreateDefaultBorder()
      const override;
  void OnThemeChanged() override;
};

}  // namespace tab_groups

#endif  // CHROME_BROWSER_UI_VIEWS_BOOKMARKS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_OVERFLOW_BUTTON_H_
