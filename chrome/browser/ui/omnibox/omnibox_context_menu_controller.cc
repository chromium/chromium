// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_context_menu_controller.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "content/public/common/url_constants.h"

OmniboxContextMenuController::OmniboxContextMenuController(
    BrowserWindowInterface* browser_window_interface)
    : browser_window_interface_(browser_window_interface) {
  menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  BuildMenu();
}

OmniboxContextMenuController::~OmniboxContextMenuController() = default;

void OmniboxContextMenuController::BuildMenu() {
  AddRecentTabItems();
}

void OmniboxContextMenuController::AddItem(int id, const std::u16string str) {
  menu_model_->AddItem(id, str);
}

void OmniboxContextMenuController::AddItem(int id, int localization_id) {
  menu_model_->AddItemWithStringId(id, localization_id);
}

void OmniboxContextMenuController::AddSeparator() {
  menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
}

void OmniboxContextMenuController::AddRecentTabItems() {
  // Iterate through the tab strip model, getting the data for each tab.
  auto* tab_strip_model = browser_window_interface_->GetTabStripModel();
  for (int i = 0; i < tab_strip_model->count(); i++) {
    TabRendererData tab_renderer_data =
        TabRendererData::FromTabInModel(tab_strip_model, i);
    const auto& last_committed_url = tab_renderer_data.last_committed_url;
    // Skip tabs that are still loading, and skip webui.
    if (!last_committed_url.is_valid() || last_committed_url.is_empty() ||
        last_committed_url.SchemeIs(content::kChromeUIScheme) ||
        last_committed_url.SchemeIs(content::kChromeUIUntrustedScheme)) {
      continue;
    }
    AddItem(i, tab_renderer_data.title);
  }
}

void OmniboxContextMenuController::ExecuteCommand(int id, int event_flags) {}
