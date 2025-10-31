// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_context_menu_controller.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/omnibox_popup_resources.h"
#include "content/public/common/url_constants.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/image/image.h"

OmniboxContextMenuController::OmniboxContextMenuController(
    BrowserWindowInterface* browser_window_interface)
    : browser_window_interface_(browser_window_interface) {
  menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  BuildMenu();
}

OmniboxContextMenuController::~OmniboxContextMenuController() = default;

void OmniboxContextMenuController::BuildMenu() {
  AddRecentTabItems();
  AddStaticItems();
}

void OmniboxContextMenuController::AddItem(int id, const std::u16string str) {
  menu_model_->AddItem(id, str);
}

void OmniboxContextMenuController::AddItemWithStringIdAndIcon(
    int id,
    int localization_id,
    const ui::ImageModel& icon) {
  menu_model_->AddItemWithStringIdAndIcon(id, localization_id, icon);
}

void OmniboxContextMenuController::AddSeparator() {
  menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
}

void OmniboxContextMenuController::AddRecentTabItems() {
  // Iterate through the tab strip model, getting the data for each tab.
  auto* tab_strip_model = browser_window_interface_->GetTabStripModel();
  size_t valid_tab_count = 0;
  AddTitleWithStringId(IDS_NTP_COMPOSE_MOST_RECENT_TABS);
  for (int i = 0; i < tab_strip_model->count(); i++) {
    TabRendererData tab_renderer_data =
        TabRendererData::FromTabInModel(tab_strip_model, i);
    const auto& last_committed_url = tab_renderer_data.last_committed_url;
    if (!IsValidTab(last_committed_url)) {
      continue;
    }
    AddItem(i, tab_renderer_data.title);
    valid_tab_count += 1;
  }
  if (!valid_tab_count) {
    auto index = menu_model_->GetIndexOfCommandId(ui::MenuModel::kTitleId);
    if (index) {
      menu_model_->RemoveItemAt(index.value());
      return;
    }
  }
  AddSeparator();
}

void OmniboxContextMenuController::AddStaticItems() {
  auto add_image_icon =
      ui::ImageModel::FromVectorIcon(kAddPhotoAlternateIcon, ui::kColorMenuIcon,
                                     ui::SimpleMenuModel::kDefaultIconSize);
  AddItemWithStringIdAndIcon(IDC_OMNIBOX_CONTEXT_ADD_IMAGE,
                             IDS_NTP_COMPOSE_ADD_IMAGE, add_image_icon);
  auto add_file_icon =
      ui::ImageModel::FromVectorIcon(kAttachFileIcon, ui::kColorMenuIcon,
                                     ui::SimpleMenuModel::kDefaultIconSize);
  AddItemWithStringIdAndIcon(IDC_OMNIBOX_CONTEXT_ADD_FILE,
                             IDS_NTP_COMPOSE_ADD_FILE, add_file_icon);
  AddSeparator();
  auto deep_search_icon =
      ui::ImageModel::FromVectorIcon(kTravelExploreIcon, ui::kColorMenuIcon,
                                     ui::SimpleMenuModel::kDefaultIconSize);
  AddItemWithStringIdAndIcon(IDC_OMNIBOX_CONTEXT_DEEP_RESEARCH,
                             IDS_NTP_COMPOSE_DEEP_SEARCH, deep_search_icon);
  auto create_images_icon = ui::ImageModel::FromResourceId(
      IDR_OMNIBOX_POPUP_IMAGES_CREATE_IMAGES_PNG);
  AddItemWithStringIdAndIcon(IDC_OMNIBOX_CONTEXT_CREATE_IMAGES,
                             IDS_NTP_COMPOSE_CREATE_IMAGES, create_images_icon);
}

void OmniboxContextMenuController::AddTitleWithStringId(int localization_id) {
  menu_model_->AddTitleWithStringId(localization_id);
}

void OmniboxContextMenuController::ExecuteCommand(int id, int event_flags) {}

bool OmniboxContextMenuController::IsValidTab(GURL url) {
  // Skip tabs that are still loading, and skip webui.
  return url.is_valid() && !url.is_empty() &&
         !url.SchemeIs(content::kChromeUIScheme) &&
         !url.SchemeIs(content::kChromeUIUntrustedScheme) &&
         !url.IsAboutBlank();
}
