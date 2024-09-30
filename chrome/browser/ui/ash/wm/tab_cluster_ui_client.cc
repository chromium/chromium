// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/wm/tab_cluster_ui_client.h"

#include "ash/public/cpp/tab_cluster/tab_cluster_ui_controller.h"
#include "ash/public/cpp/tab_cluster/tab_cluster_ui_item.h"
#include "base/containers/contains.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "content/public/browser/web_contents.h"

namespace {

// Generate tab item info from a web contents.
ash::TabClusterUIItem::Info GenerateTabItemInfo(
    content::WebContents* web_contents) {
  ash::TabClusterUIItem::Info info;
  info.title = base::UTF16ToUTF8(web_contents->GetTitle());
  info.source = web_contents->GetVisibleURL().spec();
  info.browser_window =
      chrome::FindBrowserWithTab(web_contents)->window()->GetNativeWindow();
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
        DCHECK(base::Contains(contents_item_map_, web_contents));
        ash::TabClusterUIItem* item = contents_item_map_[web_contents];
        contents_item_map_.erase(web_contents);
        controller_->RemoveTabItem(item);
      }
      break;
    case TabStripModelChange::kReplaced:
      // Update the item whose corresponding contents are replaced.
      {
        auto* replace = change.GetReplace();
        DCHECK(base::Contains(contents_item_map_, replace->old_contents));
        auto* item = contents_item_map_[replace->old_contents].get();

        item->Init(GenerateTabItemInfo(replace->new_contents));
        controller_->UpdateTabItem(item);

        contents_item_map_[replace->new_contents] = item;
        contents_item_map_.erase(replace->old_contents);
      }
      break;
    case TabStripModelChange::kMoved:
      break;
    case TabStripModelChange::kSelectionOnly:
      break;
  }
  if (selection.active_tab_changed() && !tab_strip_model->empty()) {
    controller_->ChangeActiveCandidate(
        contents_item_map_[selection.old_contents],
        contents_item_map_[selection.new_contents]);
  }
}

void TabClusterUIClient::TabChangedAt(content::WebContents* contents,
                                      int index,
                                      TabChangeType change_type) {
  DCHECK(base::Contains(contents_item_map_, contents));
  auto* item = contents_item_map_[contents].get();
  item->Init(GenerateTabItemInfo(contents));
  controller_->UpdateTabItem(item);
}
