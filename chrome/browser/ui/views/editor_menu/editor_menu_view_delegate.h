// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_EDITOR_MENU_VIEW_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_EDITOR_MENU_VIEW_DELEGATE_H_

#include <string_view>

#include "ui/views/widget/widget.h"

namespace chromeos::editor_menu {

class EditorMenuViewDelegate {
 public:
  virtual ~EditorMenuViewDelegate() = default;

  virtual void OnSettingsButtonPressed() = 0;

  virtual void OnChipButtonPressed(std::string_view text_query_id) = 0;

  virtual void OnTextfieldArrowButtonPressed(std::u16string_view text) = 0;

  virtual void OnPromoCardWidgetClosed(
      views::Widget::ClosedReason closed_reason) = 0;

  virtual void OnEditorMenuVisibilityChanged(bool visible) = 0;
};

}  // namespace chromeos::editor_menu

#endif  // CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_EDITOR_MENU_VIEW_DELEGATE_H_
