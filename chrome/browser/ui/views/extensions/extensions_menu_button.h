// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_BUTTON_H_

#include <memory>
#include <string_view>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/extensions/extension_context_menu_controller.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

// ExtensionsMenuButton is the single extension action button within a row in
// the extensions menu. This includes the extension icon and name and triggers
// the extension action.
class ExtensionsMenuButton : public HoverButton {
  METADATA_HEADER(ExtensionsMenuButton, HoverButton)

 public:
  ExtensionsMenuButton(Browser* browser, ToolbarActionViewModel* model);
  ExtensionsMenuButton(const ExtensionsMenuButton&) = delete;
  ExtensionsMenuButton& operator=(const ExtensionsMenuButton&) = delete;
  ~ExtensionsMenuButton() override;

  // HoverButton:
  void AddedToWidget() override;

  std::u16string_view label_text_for_testing() const {
    return label()->GetText();
  }

 private:
  void UpdateState();
  content::WebContents* GetCurrentWebContents() const;
  void ButtonPressed();

  const raw_ptr<Browser, DanglingUntriaged> browser_;

  // Responsible for executing the extension's actions.
  const raw_ptr<ToolbarActionViewModel, DanglingUntriaged> model_;

  // Subscription to model updates.
  base::CallbackListSubscription model_subscription_;
};

BEGIN_VIEW_BUILDER(/* no export */, ExtensionsMenuButton, HoverButton)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(/* no export */, ExtensionsMenuButton)

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_BUTTON_H_
