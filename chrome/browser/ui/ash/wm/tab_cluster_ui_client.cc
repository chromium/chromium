// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/wm/tab_cluster_ui_client.h"

#include "ash/public/cpp/tab_cluster/tab_cluster_ui_controller.h"
#include "ash/public/cpp/tab_cluster/tab_cluster_ui_item.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/browser_delegate/browser_controller.h"
#include "chrome/browser/ash/browser_delegate/browser_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/web_contents.h"

namespace {

// Generate tab item info from a web contents.
ash::TabClusterUIItem::Info GenerateTabItemInfo(
    content::WebContents* web_contents) {
  ash::TabClusterUIItem::Info info;
  info.title = base::UTF16ToUTF8(web_contents->GetTitle());
  info.source = web_contents->GetVisibleURL().possibly_invalid_spec();
  info.browser_window = ash::BrowserController::GetInstance()
                            ->GetBrowserForTab(web_contents)
                            ->GetNativeWindow();
  info.is_loading = web_contents->ShouldShowLoadingUI();
  return info;
}

}  //  namespace

TabClusterUIClient::TabClusterUIClient(ash::TabClusterUIController* controller)
    : controller_(controller), browser_tab_strip_tracker_(this, nullptr) {
  browser_tab_strip_tracker_.Init();
}

TabClusterUIClient::~TabClusterUIClient() = default;

void TabClusterUIClient::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  switch (change.type()) {
    case TabStripModelChange::kInserted:
      // Add new items corresponding to the inserted web contents.
      for (const auto& contents : change.GetInsert()->contents) {
        content::WebContents* web_contents = contents.contents;
        auto* item =
            controller_->AddTabItem(std::make_unique<ash::TabClusterUIItem>(
                GenerateTabItemInfo(web_contents)));
        contents_item_map_[web_contents] = item;
      }
      break;
    case TabStripModelChange::kRemoved:
      // Remove the items corresponding to the removed web contents.
      for (const auto& contents : change.GetRemove()->contents) {
        content::WebContents* web_contents = contents.contents;
        auto it = contents_item_map_.find(web_contents);
        DCHECK(it != contents_item_map_.end());
        ash::TabClusterUIItem* item = it->second;
        contents_item_map_.erase(it);
        controller_->RemoveTabItem(item);
      }
      break;
    case TabStripModelChange::kReplaced:
      // Update the item whose corresponding contents are replaced.
      {
        auto* replace = change.GetReplace();
        auto old_contents_it = contents_item_map_.find(replace->old_contents);
        DCHECK(old_contents_it != contents_item_map_.end());
        auto* item = old_contents_it->second.get();

        item->Init(GenerateTabItemInfo(replace->new_contents));
        controller_->UpdateTabItem(item);

        contents_item_map_[replace->new_contents] = item;
        contents_item_map_.erase(old_contents_it);
      }
      break;
    case TabStripModelChange::kMoved:
      break;
    case TabStripModelChange::kSelectionOnly:
      break;
  }
  if (selection.active_tab_changed() && !tab_strip_model->empty()) {
    auto it = contents_item_map_.find(selection.new_contents);
    auto* old_active_item =
        it != contents_item_map_.end() ? it->second.get() : nullptr;
    auto* new_active_item = contents_item_map_[selection.new_contents].get();
    controller_->ChangeActiveCandidate(old_active_item, new_active_item);
  }
}

void TabClusterUIClient::OnTabChangedAt(tabs::TabInterface* tab,
                                        int index,
                                        TabChangeType change_type) {
  content::WebContents* contents = tab->GetContents();
  // Some tests may manually add tabs to browser such that the newly added tabs
  // may start loading before being inserted into the tab strip.
  auto it = contents_item_map_.find(contents);
  if (it == contents_item_map_.end()) {
    return;
  }
  auto* item = it->second.get();

  // If there is only loading progress change, we only update item when the
  // state changes between loading and loaded.
  if (change_type == TabChangeType::kLoadingOnly &&
      item->current_info().is_loading == contents->ShouldShowLoadingUI()) {
    return;
  }
  item->Init(GenerateTabItemInfo(contents));
  controller_->UpdateTabItem(item);
}
