// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/browser_status_monitor.h"

#include <memory>

#include "ash/public/cpp/shelf_types.h"
#include "base/macros.h"
#include "chrome/browser/ui/ash/launcher/browser_shortcut_launcher_item_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

namespace {

bool IsV1WindowedApp(Browser* browser) {
  if (!browser->is_type_popup() || !browser->is_app())
    return false;
  // Crostini terminal windows do not have an app id and are handled by
  // CrostiniAppWindowShelfController. All other app windows should have a non
  // empty app id.
  return !web_app::GetAppIdFromApplicationName(browser->app_name()).empty();
}

}  // namespace

// This class monitors the WebContent of the all tab and notifies a navigation
// to the BrowserStatusMonitor.
class BrowserStatusMonitor::LocalWebContentsObserver
    : public content::WebContentsObserver {
 public:
  LocalWebContentsObserver(content::WebContents* contents,
                           BrowserStatusMonitor* monitor)
      : content::WebContentsObserver(contents), monitor_(monitor) {}

  ~LocalWebContentsObserver() override = default;

  // content::WebContentsObserver
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (!navigation_handle->IsInMainFrame() ||
        !navigation_handle->HasCommitted())
      return;

    monitor_->UpdateAppItemState(web_contents(), false /*remove*/);
    monitor_->UpdateBrowserItemState();

    // Navigating may change the ShelfID associated with the WebContents.
    Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
    if (browser &&
        browser->tab_strip_model()->GetActiveWebContents() == web_contents()) {
      monitor_->SetShelfIDForBrowserWindowContents(browser, web_contents());
    }
  }

  void WebContentsDestroyed() override {
    // We can only come here when there was a non standard termination like
    // an app got un-installed while running, etc.
    monitor_->WebContentsDestroyed(web_contents());
    // |this| is gone now.
  }

 private:
  BrowserStatusMonitor* monitor_;

  DISALLOW_COPY_AND_ASSIGN(LocalWebContentsObserver);
};

BrowserStatusMonitor::BrowserStatusMonitor(
    ChromeLauncherController* launcher_controller)
    : launcher_controller_(launcher_controller),
      browser_tab_strip_tracker_(this, this, this) {
  DCHECK(launcher_controller_);
}

BrowserStatusMonitor::~BrowserStatusMonitor() {
  browser_tab_strip_tracker_.StopObservingAndSendOnBrowserRemoved();
}

void BrowserStatusMonitor::Initialize() {
  DCHECK(!initialized_);
  initialized_ = true;
  browser_tab_strip_tracker_.Init();
}

void BrowserStatusMonitor::UpdateAppItemState(content::WebContents* contents,
                                              bool remove) {
  DCHECK(contents);
  DCHECK(initialized_);
  // It is possible to come here from Browser::SwapTabContent where the contents
  // cannot be associated with a browser. A removal however should be properly
  // processed.
  Browser* browser = chrome::FindBrowserWithWebContents(contents);
  if (remove || (browser && multi_user_util::IsProfileFromActiveUser(
                                browser->profile()))) {
    launcher_controller_->UpdateAppState(contents, remove);
  }
}

void BrowserStatusMonitor::UpdateBrowserItemState() {
  DCHECK(initialized_);
  launcher_controller_->GetBrowserShortcutLauncherItemController()
      ->UpdateBrowserItemState();
}

bool BrowserStatusMonitor::ShouldTrackBrowser(Browser* browser) {
  return true;
}

void BrowserStatusMonitor::OnBrowserAdded(Browser* browser) {
  DCHECK(initialized_);
  if (IsV1WindowedApp(browser)) {
    // Note: A V1 application will set the tab strip observer when the app gets
    // added to the shelf. This makes sure that in the multi user case we will
    // only set the observer while the app item exists in the shelf.
    AddV1AppToShelf(browser);
  }
}

void BrowserStatusMonitor::OnBrowserRemoved(Browser* browser) {
  DCHECK(initialized_);
  if (IsV1WindowedApp(browser))
    RemoveV1AppFromShelf(browser);

  UpdateBrowserItemState();
}

void BrowserStatusMonitor::ActiveTabChanged(content::WebContents* old_contents,
                                            content::WebContents* new_contents,
                                            int index,
                                            int reason) {
  Browser* browser = NULL;
  // Use |new_contents|. |old_contents| could be NULL.
  DCHECK(new_contents);
  browser = chrome::FindBrowserWithWebContents(new_contents);

  // Update immediately on a tab change.
  if (old_contents &&
      (TabStripModel::kNoTab !=
       browser->tab_strip_model()->GetIndexOfWebContents(old_contents))) {
    UpdateAppItemState(old_contents, false /*remove*/);
  }

  if (new_contents) {
    UpdateAppItemState(new_contents, false /*remove*/);
    UpdateBrowserItemState();
    SetShelfIDForBrowserWindowContents(browser, new_contents);
  }
}

void BrowserStatusMonitor::TabReplacedAt(TabStripModel* tab_strip_model,
                                         content::WebContents* old_contents,
                                         content::WebContents* new_contents,
                                         int index) {
  DCHECK(old_contents && new_contents);
  Browser* browser = chrome::FindBrowserWithWebContents(new_contents);

  UpdateAppItemState(old_contents, true /*remove*/);
  RemoveWebContentsObserver(old_contents);

  UpdateAppItemState(new_contents, false /*remove*/);
  UpdateBrowserItemState();

  if (tab_strip_model->GetActiveWebContents() == new_contents)
    SetShelfIDForBrowserWindowContents(browser, new_contents);

  AddWebContentsObserver(new_contents);
}

void BrowserStatusMonitor::TabInsertedAt(TabStripModel* tab_strip_model,
                                         content::WebContents* contents,
                                         int index,
                                         bool foreground) {
  UpdateAppItemState(contents, false /*remove*/);
  AddWebContentsObserver(contents);
}

void BrowserStatusMonitor::TabClosingAt(TabStripModel* tab_strip_mode,
                                        content::WebContents* contents,
                                        int index) {
  UpdateAppItemState(contents, true /*remove*/);
  RemoveWebContentsObserver(contents);
}

void BrowserStatusMonitor::WebContentsDestroyed(
    content::WebContents* contents) {
  UpdateAppItemState(contents, true /*remove*/);
  RemoveWebContentsObserver(contents);
}

void BrowserStatusMonitor::AddV1AppToShelf(Browser* browser) {
  DCHECK(IsV1WindowedApp(browser));
  DCHECK(initialized_);

  std::string app_id =
      web_app::GetAppIdFromApplicationName(browser->app_name());
  DCHECK(!app_id.empty());
  if (!IsV1AppInShelfWithAppId(app_id))
    launcher_controller_->SetV1AppStatus(app_id, ash::STATUS_RUNNING);
  browser_to_app_id_map_[browser] = app_id;
}

void BrowserStatusMonitor::RemoveV1AppFromShelf(Browser* browser) {
  DCHECK(IsV1WindowedApp(browser));
  DCHECK(initialized_);

  auto iter = browser_to_app_id_map_.find(browser);
  if (iter != browser_to_app_id_map_.end()) {
    std::string app_id = iter->second;
    browser_to_app_id_map_.erase(iter);
    if (!IsV1AppInShelfWithAppId(app_id))
      launcher_controller_->SetV1AppStatus(app_id, ash::STATUS_CLOSED);
  }
}

bool BrowserStatusMonitor::IsV1AppInShelf(Browser* browser) {
  return browser_to_app_id_map_.find(browser) != browser_to_app_id_map_.end();
}

bool BrowserStatusMonitor::IsV1AppInShelfWithAppId(const std::string& app_id) {
  for (const auto& iter : browser_to_app_id_map_) {
    if (iter.second == app_id)
      return true;
  }
  return false;
}

void BrowserStatusMonitor::AddWebContentsObserver(
    content::WebContents* contents) {
  if (webcontents_to_observer_map_.find(contents) ==
      webcontents_to_observer_map_.end()) {
    webcontents_to_observer_map_[contents] =
        std::make_unique<LocalWebContentsObserver>(contents, this);
  }
}

void BrowserStatusMonitor::RemoveWebContentsObserver(
    content::WebContents* contents) {
  DCHECK(webcontents_to_observer_map_.find(contents) !=
         webcontents_to_observer_map_.end());
  webcontents_to_observer_map_.erase(contents);
}

ash::ShelfID BrowserStatusMonitor::GetShelfIDForWebContents(
    content::WebContents* contents) {
  return launcher_controller_->GetShelfIDForWebContents(contents);
}

void BrowserStatusMonitor::SetShelfIDForBrowserWindowContents(
    Browser* browser,
    content::WebContents* web_contents) {
  launcher_controller_->GetBrowserShortcutLauncherItemController()
      ->SetShelfIDForBrowserWindowContents(browser, web_contents);
}
