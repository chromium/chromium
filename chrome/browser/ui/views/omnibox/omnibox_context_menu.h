// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_CONTEXT_MENU_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_CONTEXT_MENU_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/omnibox/omnibox_context_menu_controller.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "ui/menus/simple_menu_model.h"

class OmniboxContextMenu : public ui::SimpleMenuModel,
                           public ui::SimpleMenuModel::Delegate {
 public:
  explicit OmniboxContextMenu(
      base::WeakPtr<TopChromeWebUIController::Embedder> embedder);

  ~OmniboxContextMenu() override;

  void ExecuteCommand(int command_id, int event_flags) override;

  void CloseMenu();

 private:
  base::WeakPtr<TopChromeWebUIController::Embedder> embedder_;
  std::unique_ptr<OmniboxContextMenuController> controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_CONTEXT_MENU_H_
