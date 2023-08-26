// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_EDITOR_MENU_VIEW_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_EDITOR_MENU_VIEW_DELEGATE_H_

#include <string>

namespace chromeos::editor_menu {

class EditorMenuViewDelegate {
 public:
  virtual ~EditorMenuViewDelegate() = default;

  virtual void OnSettingsButtonPressed() = 0;

  virtual void OnChipButtonPressed(int button_id,
                                   const std::u16string& text) = 0;

  virtual void OnTextfieldArrowButtonPressed(const std::u16string& text) = 0;

  virtual void OnPromoCardDismissButtonPressed() = 0;

  virtual void OnPromoCardTellMeMoreButtonPressed() = 0;
};

}  // namespace chromeos::editor_menu

#endif  // CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_EDITOR_MENU_VIEW_DELEGATE_H_
