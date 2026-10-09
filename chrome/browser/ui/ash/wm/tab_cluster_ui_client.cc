// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/wm/tab_cluster_ui_client.h"

#include <utility>

#include "ash/public/cpp/tab_cluster/tab_cluster_ui_controller.h"
#include "ash/public/cpp/tab_cluster/tab_cluster_ui_item.h"
#include "base/check.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/components/browser_delegate/browser_controller.h"
#include "chromeos/ash/components/browser_delegate/browser_delegate.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

namespace {

// Generate tab item info from a web contents.
ash::TabClusterUIItem::Info GenerateTabItemInfo(
    content::WebContents* web_contents,
    aura::Window* browser_window) {
  ash::TabClusterUIItem::Info info;
  info.title = base::UTF16ToUTF8(web_contents->GetTitle());
  // Unlike the visible URL, the last committed URL never reflects a pending
  // navigation (which may never commit).
  info.source = web_contents->GetLastCommittedURL().possibly_invalid_spec();
  info.browser_window = browser_window;
  info.is_loading = web_contents->ShouldShowLoadingUI();
  return info;
}

}  // namespace

class TabClusterUIClient::TrackedTab : public content::WebContentsObserver {
 public:
  TrackedTab(TabClusterUIClient* client, ash::TabClusterUIItem* item)
      : client_(client), item_(item) {}
  ~TrackedTab() override = default;

  using content::WebContentsObserver::Observe;

  ash::TabClusterUIItem* item() const { return item_; }

  // Regenerates the item's info and notifies the controller if it changed.
  void Update() {
    CHECK(web_contents());
    const ash::TabClusterUIItem::Info& current_info = item_->current_info();
    // Reuse the browser window determined at insertion. It can't change while
    // the tab is tracked: moving the tab to another browser untracks it
    // (kRemoved) and tracks it anew (kInserted), and replacing its contents
    // (kReplaced) keeps it in the same tab strip.
    ash::TabClusterUIItem::Info new_info =
        GenerateTabItemInfo(web_contents(), current_info.browser_window);
    if (new_info.title == current_info.title &&
        new_info.source == current_info.source &&
        new_info.is_loading == current_info.is_loading) {
      return;
    }
    item_->Init(new_info);
    client_->controller_->UpdateTabItem(item_);
  }

  // content::WebContentsObserver:
  void DidFinishNavigation(content::NavigationHandle* handle) override {
    // The source only changes when a primary main frame navigation commits.
    if (!handle->IsInPrimaryMainFrame() || !handle->HasCommitted()) {
      return;
    }
    Update();
  }

  void TitleWasSet(content::NavigationEntry* entry) override {
    // No need to filter by `entry`, as Update() ignores no-op changes.
    Update();
  }

 private:
  const raw_ptr<TabClusterUIClient> client_;
  const raw_ptr<ash::TabClusterUIItem> item_;
};

TabClusterUIClient::TabClusterUIClient(ash::TabClusterUIController* controller)
    : controller_(controller), browser_tab_strip_tracker_(this, nullptr) {
  browser_tab_strip_tracker_.Init();
  tab_observation_.Observe(ash::BrowserController::GetInstance());
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
        ash::BrowserDelegate* browser =
            ash::BrowserController::GetInstance()->GetBrowserForTab(
                web_contents);
        CHECK(browser);
        auto* item =
            controller_->AddTabItem(std::make_unique<ash::TabClusterUIItem>(
                GenerateTabItemInfo(web_contents, browser->GetNativeWindow())));
        auto tracked = std::make_unique<TrackedTab>(this, item);
        tracked->Observe(web_contents);
        tabs_[web_contents] = std::move(tracked);
      }
      break;
    case TabStripModelChange::kRemoved:
      // Remove the items corresponding to the removed web contents.
      for (const auto& contents : change.GetRemove()->contents) {
        content::WebContents* web_contents = contents.contents;
        auto it = tabs_.find(web_contents);
        CHECK(it != tabs_.end());
        ash::TabClusterUIItem* item = it->second->item();
        tabs_.erase(it);
        controller_->RemoveTabItem(item);
      }
      break;
    case TabStripModelChange::kReplaced: {
      // Update the item whose corresponding contents are replaced.
      auto* replace = change.GetReplace();
      auto old_contents_it = tabs_.find(replace->old_contents);
      CHECK(old_contents_it != tabs_.end());
      std::unique_ptr<TrackedTab> tracked = std::move(old_contents_it->second);
      tabs_.erase(old_contents_it);

      tracked->Observe(replace->new_contents);
      tracked->Update();
      tabs_[replace->new_contents] = std::move(tracked);
      break;
    }
    case TabStripModelChange::kMoved:
    case TabStripModelChange::kSelectionOnly:
      break;
  }
}

void TabClusterUIClient::OnTabLoadingStateChanged(
    ash::BrowserDelegate* browser,
    content::WebContents* contents) {
  auto it = tabs_.find(contents);
  if (it == tabs_.end()) {
    return;
  }
  it->second->Update();
}
