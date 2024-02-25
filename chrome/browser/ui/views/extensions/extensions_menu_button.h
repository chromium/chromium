// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_BUTTON_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/extensions/extension_context_menu_controller.h"
#include "chrome/browser/ui/views/toolbar/toolbar_action_view_delegate_views.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace views {
class Button;
}  // namespace views

// ExtensionsMenuButton is the single extension action button within a row in
// the extensions menu. This includes the extension icon and name and triggers
// the extension action.
class ExtensionsMenuButton : public HoverButton,
                             public ToolbarActionViewDelegateViews {
  METADATA_HEADER(ExtensionsMenuButton, HoverButton)

 public:
  ExtensionsMenuButton(Browser* browser,
                       ToolbarActionViewController* controller);
  ExtensionsMenuButton(const ExtensionsMenuButton&) = delete;
  ExtensionsMenuButton& operator=(const ExtensionsMenuButton&) = delete;
  ~ExtensionsMenuButton() override;

  // HoverButton:
  void AddedToWidget() override;

  const std::u16string& label_text_for_testing() const {
    return label()->GetText();
  }

 private:
  // ToolbarActionViewDelegateViews:
  views::FocusManager* GetFocusManagerForAccelerator() override;
  views::Button* GetReferenceButtonForPopup() override;
  content::WebContents* GetCurrentWebContents() const override;
  void UpdateState() override;
  void ShowContextMenuAsFallback() override;

  void ButtonPressed();

  const raw_ptr<Browser, DanglingUntriaged> browser_;

  // Responsible for executing the extension's actions.
  const raw_ptr<ToolbarActionViewController, DanglingUntriaged> controller_;
};

BEGIN_VIEW_BUILDER(/* no export */, ExtensionsMenuButton, HoverButton)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(/* no export */, ExtensionsMenuButton)

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_BUTTON_H_
