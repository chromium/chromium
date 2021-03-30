// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/browser_status_monitor.h"

#include <memory>

#include "ash/public/cpp/shelf_types.h"
#include "base/containers/contains.h"
#include "base/macros.h"
#include "chrome/browser/ui/ash/launcher/app_service/app_service_app_window_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller_util.h"
#include "chrome/browser/ui/ash/launcher/shelf_spinner_controller.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

namespace {

bool IsV1WindowedApp(Browser* browser) {
  if (!browser->deprecated_is_app())
    return false;
  // Crostini terminal windows do not have an app id and are handled by
  // CrostiniAppWindowShelfController. All other app windows should have a non
  // empty app id.
  return !web_app::GetAppIdFromApplicationName(browser->app_name()).empty();
}

#if DCHECK_IS_ON()
Browser* GetBrowserWithTabStripModel(TabStripModel* tab_strip_model) {
  for (auto* browser : *BrowserList::GetInstance()) {
    if (browser->tab_strip_model() == tab_strip_model)
      return browser;
  }
  return nullptr;
}
#endif  // DCHECK_IS_ON()

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
      browser_tab_strip_tracker_(this, nullptr) {
  DCHECK(launcher_controller_);

  app_service_instance_helper_ =
      launcher_controller->app_service_app_window_controller()
          ->app_service_instance_helper();
  DCHECK(app_service_instance_helper_);
}

BrowserStatusMonitor::~BrowserStatusMonitor() {
  DCHECK(initialized_);

  BrowserList::RemoveObserver(this);

  // Simulate OnBrowserRemoved() for all Browsers.
  for (auto* browser : *BrowserList::GetInstance())
    OnBrowserRemoved(browser);
}

void BrowserStatusMonitor::Initialize() {
  DCHECK(!initialized_);
  initialized_ = true;

  // Simulate OnBrowserAdded() for all existing Browsers.
  for (auto* browser : *BrowserList::GetInstance())
    OnBrowserAdded(browser);

  // BrowserList::AddObserver() comes before BrowserTabStripTracker::Init() to
  // ensure that OnBrowserAdded() is always invoked before
  // OnTabStripModelChanged() is invoked to describe the initial state of the
  // Browser.
  BrowserList::AddObserver(this);
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
  launcher_controller_->UpdateBrowserItemState();
}

void BrowserStatusMonitor::OnBrowserAdded(Browser* browser) {
  DCHECK(initialized_);
#if DCHECK_IS_ON()
  auto insert_result = known_browsers_.insert(browser);
  DCHECK(insert_result.second);
#endif

  if (IsV1WindowedApp(browser)) {
    // Note: A V1 application will set the tab strip observer when the app gets
    // added to the shelf. This makes sure that in the multi user case we will
    // only set the observer while the app item exists in the shelf.
    AddV1AppToShelf(browser);
  }
}

void BrowserStatusMonitor::OnBrowserRemoved(Browser* browser) {
  DCHECK(initialized_);
#if DCHECK_IS_ON()
  size_t num_removed = known_browsers_.erase(browser);
  DCHECK_EQ(num_removed, 1U);
#endif

  if (IsV1WindowedApp(browser))
    RemoveV1AppFromShelf(browser);

  UpdateBrowserItemState();
  if (app_service_instance_helper_)
    app_service_instance_helper_->OnBrowserRemoved();
}

void BrowserStatusMonitor::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  // OnBrowserAdded() must be invoked before OnTabStripModelChanged(). See
  // comment in constructor.
#if DCHECK_IS_ON()
  {
    Browser* browser = GetBrowserWithTabStripModel(tab_strip_model);
    DCHECK(base::Contains(known_browsers_, browser));
  }
#endif

  if (change.type() == TabStripModelChange::kInserted) {
    for (const auto& contents : change.GetInsert()->contents)
      OnTabInserted(tab_strip_model, contents.contents);
    UpdateBrowserItemState();
  } else if (change.type() == TabStripModelChange::kRemoved) {
    auto* remove = change.GetRemove();
    for (const auto& contents : remove->contents) {
      if (contents.will_be_deleted)
        OnTabClosing(contents.contents);
    }
  } else if (change.type() == TabStripModelChange::kReplaced) {
    auto* replace = change.GetReplace();
    OnTabReplaced(tab_strip_model, replace->old_contents,
                  replace->new_contents);
  }

  if (tab_strip_model->empty())
    return;

  if (selection.active_tab_changed())
    OnActiveTabChanged(selection.old_contents, selection.new_contents);
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
  if (!IsV1AppInShelfWithAppId(app_id)) {
    if (auto* chrome_controller = ChromeLauncherController::instance()) {
      chrome_controller->GetShelfSpinnerController()->CloseSpinner(app_id);
    }
    launcher_controller_->SetV1AppStatus(app_id, ash::STATUS_RUNNING);
  }
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

void BrowserStatusMonitor::OnActiveTabChanged(
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  Browser* browser = nullptr;
  // Use |new_contents|. |old_contents| could be nullptr.
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

  if (app_service_instance_helper_) {
    app_service_instance_helper_->OnActiveTabChanged(old_contents,
                                                     new_contents);
  }
}

void BrowserStatusMonitor::OnTabReplaced(TabStripModel* tab_strip_model,
                                         content::WebContents* old_contents,
                                         content::WebContents* new_contents) {
  DCHECK(old_contents && new_contents);
  Browser* browser = chrome::FindBrowserWithWebContents(new_contents);

  UpdateAppItemState(old_contents, true /*remove*/);
  RemoveWebContentsObserver(old_contents);

  UpdateAppItemState(new_contents, false /*remove*/);
  UpdateBrowserItemState();

  if (browser && IsV1AppInShelf(browser) &&
      multi_user_util::IsProfileFromActiveUser(browser->profile())) {
    launcher_controller_->SetV1AppStatus(
        web_app::GetAppIdFromApplicationName(browser->app_name()),
        ash::STATUS_RUNNING);
  }

  if (tab_strip_model->GetActiveWebContents() == new_contents)
    SetShelfIDForBrowserWindowContents(browser, new_contents);

  AddWebContentsObserver(new_contents);

  if (app_service_instance_helper_)
    app_service_instance_helper_->OnTabReplaced(old_contents, new_contents);
}

void BrowserStatusMonitor::OnTabInserted(TabStripModel* tab_strip_model,
                                         content::WebContents* contents) {
  UpdateAppItemState(contents, false /*remove*/);
  // If the contents does not have a visible navigation entry, wait until a
  // navigation status changes before setting the browser window Shelf ID
  // (done by the web contents observer added by AddWebContentsObserver()).
  if (tab_strip_model->GetActiveWebContents() == contents &&
      contents->GetController().GetVisibleEntry()) {
    Browser* browser = chrome::FindBrowserWithWebContents(contents);
    SetShelfIDForBrowserWindowContents(browser, contents);
  }

  AddWebContentsObserver(contents);
  if (app_service_instance_helper_)
    app_service_instance_helper_->OnTabInserted(contents);
}

void BrowserStatusMonitor::OnTabClosing(content::WebContents* contents) {
  UpdateAppItemState(contents, true /*remove*/);
  RemoveWebContentsObserver(contents);
  if (app_service_instance_helper_)
    app_service_instance_helper_->OnTabClosing(contents);
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

void BrowserStatusMonitor::SetShelfIDForBrowserWindowContents(
    Browser* browser,
    content::WebContents* web_contents) {
  launcher_controller_->SetShelfIDForBrowserWindowContents(browser,
                                                           web_contents);

  if (app_service_instance_helper_) {
    app_service_instance_helper_->OnSetShelfIDForBrowserWindowContents(
        web_contents);
  }
}
