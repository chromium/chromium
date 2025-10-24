// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_context_menu.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/omnibox/omnibox_context_menu_controller.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "ui/menus/simple_menu_model.h"

OmniboxContextMenu::OmniboxContextMenu(
    base::WeakPtr<TopChromeWebUIController::Embedder> embedder)
    : ui::SimpleMenuModel(this),
      embedder_(embedder),
      controller_(base::WrapUnique(new OmniboxContextMenuController())) {}

OmniboxContextMenu::~OmniboxContextMenu() = default;

void OmniboxContextMenu::ExecuteCommand(int command_id, int event_flags) {
  controller_->ExecuteCommand(command_id, event_flags);
}

void OmniboxContextMenu::CloseMenu() {
  if (embedder_) {
    embedder_->HideContextMenu();
  }
}
