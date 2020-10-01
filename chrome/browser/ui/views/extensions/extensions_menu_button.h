// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_BUTTON_H_

#include <memory>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/views/extensions/extension_context_menu_controller.h"
#include "chrome/browser/ui/views/toolbar/toolbar_action_view_delegate_views.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/label_button.h"

class ExtensionsMenuItemView;

namespace views {
class Button;
}  // namespace views

// ExtensionsMenuButton is the single extension action button within a row in
// the extensions menu. This includes the extension icon and name and triggers
// the extension action.
class ExtensionsMenuButton : public views::LabelButton,
                             public ToolbarActionViewDelegateViews {
 public:
  ExtensionsMenuButton(Browser* browser,
                       ExtensionsMenuItemView* parent,
                       ToolbarActionViewController* controller,
                       bool allow_pinning);
  ExtensionsMenuButton(const ExtensionsMenuButton&) = delete;
  ExtensionsMenuButton& operator=(const ExtensionsMenuButton&) = delete;
  ~ExtensionsMenuButton() override;

  static const char kClassName[];

  SkColor GetInkDropBaseColor() const override;
  bool CanShowIconInToolbar() const override;

  const base::string16& label_text_for_testing() const {
    return label()->GetText();
  }

 private:
  // views::ButtonListener:
  const char* GetClassName() const override;

  // ToolbarActionViewDelegateViews:
  views::View* GetAsView() override;
  views::FocusManager* GetFocusManagerForAccelerator() override;
  views::Button* GetReferenceButtonForPopup() override;
  content::WebContents* GetCurrentWebContents() const override;
  void UpdateState() override;
  bool IsMenuRunning() const override;

  void ButtonPressed();

  Browser* const browser_;

  // The container containing this view.
  ExtensionsMenuItemView* const parent_;

  // Responsible for executing the extension's actions.
  ToolbarActionViewController* const controller_;

  bool allow_pinning_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_BUTTON_H_
