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
    // (OnTabRemoved) and tracks it anew (OnTabInserted), and replacing its
    // contents (OnTabReplaced) keeps it in the same browser.
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
    : controller_(controller) {
  tab_observation_.Observe(ash::BrowserController::GetInstance());
}

TabClusterUIClient::~TabClusterUIClient() = default;

void TabClusterUIClient::OnTabInserted(ash::BrowserDelegate* browser,
                                       content::WebContents* contents) {
  auto* item = controller_->AddTabItem(std::make_unique<ash::TabClusterUIItem>(
      GenerateTabItemInfo(contents, browser->GetNativeWindow())));
  auto tracked = std::make_unique<TrackedTab>(this, item);
  tracked->Observe(contents);
  tabs_[contents] = std::move(tracked);
}

void TabClusterUIClient::OnTabRemoved(ash::BrowserDelegate* browser,
                                      content::WebContents* contents,
                                      bool will_delete) {
  auto it = tabs_.find(contents);
  CHECK(it != tabs_.end());
  ash::TabClusterUIItem* item = it->second->item();
  tabs_.erase(it);
  controller_->RemoveTabItem(item);
}

void TabClusterUIClient::OnTabReplaced(ash::BrowserDelegate* browser,
                                       content::WebContents* old_contents,
                                       content::WebContents* new_contents) {
  auto it = tabs_.find(old_contents);
  CHECK(it != tabs_.end());
  std::unique_ptr<TrackedTab> tracked = std::move(it->second);
  tabs_.erase(it);

  tracked->Observe(new_contents);
  tracked->Update();
  tabs_[new_contents] = std::move(tracked);
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
