// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/browser_status_monitor.h"

#include <memory>

#include "ash/public/cpp/shelf_types.h"
#include "base/containers/contains.h"
#include "base/debug/dump_without_crashing.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/shelf/app_service/app_service_app_window_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller_util.h"
#include "chrome/browser/ui/ash/shelf/shelf_spinner_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

namespace {

// Checks if a given browser is running a windowed app. It will return true for
// web apps, hosted apps, and packaged V1 apps.
bool IsAppBrowser(const Browser* browser) {
  return (browser->is_type_app() || browser->is_type_app_popup()) &&
         !web_app::GetAppIdFromApplicationName(browser->app_name()).empty();
}

Browser* GetBrowserWithTabStripModel(TabStripModel* tab_strip_model) {
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser->tab_strip_model() == tab_strip_model)
      return browser;
  }
  return nullptr;
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

  LocalWebContentsObserver(const LocalWebContentsObserver&) = delete;
  LocalWebContentsObserver& operator=(const LocalWebContentsObserver&) = delete;

  ~LocalWebContentsObserver() override = default;

  // content::WebContentsObserver
  void PrimaryPageChanged(content::Page& page) override {
    monitor_->OnTabNavigationFinished(web_contents());
  }

 private:
  raw_ptr<BrowserStatusMonitor> monitor_;
};

BrowserStatusMonitor::BrowserStatusMonitor(
    ChromeShelfController* shelf_controller)
    : shelf_controller_(shelf_controller),
      browser_tab_strip_tracker_(this, nullptr) {
  DCHECK(shelf_controller_);

  app_service_instance_helper_ =
      shelf_controller->app_service_app_window_controller()
          ->app_service_instance_helper();
  DCHECK(app_service_instance_helper_);
}

BrowserStatusMonitor::~BrowserStatusMonitor() {
  DCHECK(initialized_);

  BrowserList::RemoveObserver(this);

  // Simulate OnBrowserRemoved() for all Browsers.
  for (Browser* browser : *BrowserList::GetInstance()) {
    OnBrowserRemoved(browser);
  }
}

void BrowserStatusMonitor::Initialize() {
  DCHECK(!initialized_);
  initialized_ = true;

  // Simulate OnBrowserAdded() for all existing Browsers.
  for (Browser* browser : *BrowserList::GetInstance()) {
    OnBrowserAdded(browser);
  }

  // BrowserList::AddObserver() comes before BrowserTabStripTracker::Init() to
  // ensure that OnBrowserAdded() is always invoked before
  // OnTabStripModelChanged() is invoked to describe the initial state of the
  // Browser.
  BrowserList::AddObserver(this);
  browser_tab_strip_tracker_.Init();
}

void BrowserStatusMonitor::ActiveUserChanged(const std::string& user_email) {
  if (web_app::IsWebAppsCrosapiEnabled()) {
    return;
  }
  // When the active profile changes, all windowed and tabbed apps owned by the
  // newly selected profile are added to the shelf, and the ones owned by other
  // profiles are removed.
  for (Browser* browser : *BrowserList::GetInstance()) {
    bool owned = multi_user_util::IsProfileFromActiveUser(browser->profile());
    TabStripModel* tab_strip_model = browser->tab_strip_model();

    if (browser->is_type_app() || browser->is_type_app_popup()) {
      // Add windowed apps owned by the current profile, and remove the one
      // owned by other profiles.
      bool app_in_shelf = IsAppBrowserInShelf(browser);
      content::WebContents* active_web_contents =
          tab_strip_model->GetActiveWebContents();

      if (owned && !app_in_shelf) {
        // Adding an app to the shelf consists of two actions: add the browser
        // (shelf item) and add the content (shelf item status).
        AddAppBrowserToShelf(browser);
        if (active_web_contents) {
          shelf_controller_->UpdateAppState(active_web_contents,
                                            false /*remove*/);
        }
      } else if (!owned && app_in_shelf) {
        // Removing an app from the shelf requires to remove the content and
        // the shelf item (reverse order of addition).
        if (active_web_contents) {
          shelf_controller_->UpdateAppState(active_web_contents,
                                            true /*remove*/);
        }
        RemoveAppBrowserFromShelf(browser);
      }

    } else if (browser->is_type_normal()) {
      // Add tabbed apps owned by the current profile, and remove the ones owned
      // by other profiles.
      for (int i = 0; i < tab_strip_model->count(); ++i) {
        shelf_controller_->UpdateAppState(tab_strip_model->GetWebContentsAt(i),
                                          !owned /*remove*/);
      }
    }
  }

  // Update the browser state since some of the additions/removals above might
  // have had an impact on the browser item state.
  UpdateBrowserItemState();
}

void BrowserStatusMonitor::UpdateAppItemState(content::WebContents* contents,
                                              bool remove) {
  DCHECK(!web_app::IsWebAppsCrosapiEnabled());
  DCHECK(contents);
  DCHECK(initialized_);
  // It is possible to come here from Browser::SwapTabContent where the contents
  // cannot be associated with a browser. A removal however should be properly
  // processed.
  Browser* browser = chrome::FindBrowserWithTab(contents);
  if (remove || (browser && multi_user_util::IsProfileFromActiveUser(
                                browser->profile()))) {
    shelf_controller_->UpdateAppState(contents, remove);
  }
}

void BrowserStatusMonitor::UpdateBrowserItemState() {
  DCHECK(!web_app::IsWebAppsCrosapiEnabled());
  DCHECK(initialized_);
  shelf_controller_->UpdateBrowserItemState();
}

void BrowserStatusMonitor::OnBrowserAdded(Browser* browser) {
  DCHECK(initialized_);
#if DCHECK_IS_ON()
  auto insert_result = known_browsers_.insert(browser);
  DCHECK(insert_result.second);
#endif

  if (!web_app::IsWebAppsCrosapiEnabled()) {
    if (IsAppBrowser(browser) &&
        multi_user_util::IsProfileFromActiveUser(browser->profile())) {
      AddAppBrowserToShelf(browser);
    }
  }
}

void BrowserStatusMonitor::OnBrowserRemoved(Browser* browser) {
  DCHECK(initialized_);
#if DCHECK_IS_ON()
  size_t num_removed = known_browsers_.erase(browser);
  DCHECK_EQ(num_removed, 1U);
#endif

  if (!web_app::IsWebAppsCrosapiEnabled()) {
    if (IsAppBrowser(browser) &&
        multi_user_util::IsProfileFromActiveUser(browser->profile())) {
      RemoveAppBrowserFromShelf(browser);
    }

    UpdateBrowserItemState();
  }
  if (app_service_instance_helper_)
    app_service_instance_helper_->OnBrowserRemoved();
}

void BrowserStatusMonitor::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  // OnBrowserAdded() must be invoked before OnTabStripModelChanged(). See
  // comment in constructor.
  Browser* browser = GetBrowserWithTabStripModel(tab_strip_model);
#if DCHECK_IS_ON()
  DCHECK(base::Contains(known_browsers_, browser));
#endif

  if (change.type() == TabStripModelChange::kInserted) {
    for (const auto& contents : change.GetInsert()->contents) {
      if (base::Contains(webcontents_to_observer_map_, contents.contents)) {
#if DCHECK_IS_ON()
        {
          // The tab must be in the set of tabs in transit.
          size_t num_removed = tabs_in_transit_.erase(contents.contents.get());
          DCHECK_EQ(num_removed, 1u);
        }
#endif
        OnTabMoved(tab_strip_model, contents.contents);
      } else {
#if DCHECK_IS_ON()
        DCHECK(!base::Contains(tabs_in_transit_, contents.contents));
#endif
        OnTabInserted(tab_strip_model, contents.contents);
      }
    }
    if (!web_app::IsWebAppsCrosapiEnabled()) {
      UpdateBrowserItemState();
    }
  } else if (change.type() == TabStripModelChange::kRemoved) {
    auto* remove = change.GetRemove();
    for (const auto& contents : remove->contents) {
      switch (contents.remove_reason) {
        case TabStripModelChange::RemoveReason::kDeleted:
#if DCHECK_IS_ON()
          DCHECK(!base::Contains(tabs_in_transit_, contents.contents));
#endif
          OnTabClosing(contents.contents);
          break;
        case TabStripModelChange::RemoveReason::kInsertedIntoOtherTabStrip:
          // The tab will be reinserted immediately into another browser, so
          // this event is ignored.
          if (browser->is_type_devtools()) {
            // TODO(crbug.com/40773744): when a dev tools window is docked, and
            // its WebContents is removed, it will not be reinserted into
            // another tab strip, so it should be treated as closed.
            OnTabClosing(contents.contents);
          } else {
#if DCHECK_IS_ON()
            // The tab must not be already in the set of tabs in transit.
            DCHECK(tabs_in_transit_.insert(contents.contents).second);
#endif
          }
          break;
      }
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

void BrowserStatusMonitor::AddAppBrowserToShelf(Browser* browser) {
  DCHECK(!web_app::IsWebAppsCrosapiEnabled());
  DCHECK(IsAppBrowser(browser));
  DCHECK(initialized_);

  std::string app_id =
      web_app::GetAppIdFromApplicationName(browser->app_name());
  DCHECK(!app_id.empty());
  if (!IsAppBrowserInShelfWithAppId(app_id)) {
    if (auto* chrome_controller = ChromeShelfController::instance()) {
      chrome_controller->GetShelfSpinnerController()->CloseSpinner(app_id);
    }
    shelf_controller_->SetAppStatus(app_id, ash::STATUS_RUNNING);
  }
  browser_to_app_id_map_[browser] = app_id;
}

void BrowserStatusMonitor::RemoveAppBrowserFromShelf(Browser* browser) {
  DCHECK(!web_app::IsWebAppsCrosapiEnabled());
  DCHECK(IsAppBrowser(browser));
  DCHECK(initialized_);

  auto iter = browser_to_app_id_map_.find(browser);
  if (iter != browser_to_app_id_map_.end()) {
    std::string app_id = iter->second;
    browser_to_app_id_map_.erase(iter);
    if (!IsAppBrowserInShelfWithAppId(app_id))
      shelf_controller_->SetAppStatus(app_id, ash::STATUS_CLOSED);
  }
}

bool BrowserStatusMonitor::IsAppBrowserInShelf(Browser* browser) {
  DCHECK(!web_app::IsWebAppsCrosapiEnabled());
  return browser_to_app_id_map_.find(browser) != browser_to_app_id_map_.end();
}

bool BrowserStatusMonitor::IsAppBrowserInShelfWithAppId(
    const std::string& app_id) {
  DCHECK(!web_app::IsWebAppsCrosapiEnabled());
  for (const auto& iter : browser_to_app_id_map_) {
    if (iter.second == app_id)
      return true;
  }
  return false;
}

void BrowserStatusMonitor::OnActiveTabChanged(
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  if (!web_app::IsWebAppsCrosapiEnabled()) {
    Browser* browser = nullptr;
    // Use |new_contents|. |old_contents| could be nullptr.
    DCHECK(new_contents);
    browser = chrome::FindBrowserWithTab(new_contents);

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

  if (app_service_instance_helper_) {
    app_service_instance_helper_->OnActiveTabChanged(old_contents,
                                                     new_contents);
  }
}

void BrowserStatusMonitor::OnTabReplaced(TabStripModel* tab_strip_model,
                                         content::WebContents* old_contents,
                                         content::WebContents* new_contents) {
  if (!web_app::IsWebAppsCrosapiEnabled()) {
    DCHECK(old_contents && new_contents);
    Browser* browser = chrome::FindBrowserWithTab(new_contents);

    UpdateAppItemState(old_contents, true /*remove*/);
    RemoveWebContentsObserver(old_contents);

    UpdateAppItemState(new_contents, false /*remove*/);
    UpdateBrowserItemState();

    if (browser && IsAppBrowserInShelf(browser) &&
        multi_user_util::IsProfileFromActiveUser(browser->profile())) {
      shelf_controller_->SetAppStatus(
          web_app::GetAppIdFromApplicationName(browser->app_name()),
          ash::STATUS_RUNNING);
    }

    if (tab_strip_model->GetActiveWebContents() == new_contents)
      SetShelfIDForBrowserWindowContents(browser, new_contents);

    AddWebContentsObserver(new_contents);
  }

  if (app_service_instance_helper_)
    app_service_instance_helper_->OnTabReplaced(old_contents, new_contents);
}

void BrowserStatusMonitor::OnTabInserted(TabStripModel* tab_strip_model,
                                         content::WebContents* contents) {
  if (!web_app::IsWebAppsCrosapiEnabled()) {
    UpdateAppItemState(contents, false /*remove*/);
    // If the visible navigation entry is the initial entry, wait until a
    // navigation status changes before setting the browser window Shelf ID
    // (done by the web contents observer added by AddWebContentsObserver()).
    if (tab_strip_model->GetActiveWebContents() == contents &&
        !contents->GetController().GetVisibleEntry()->IsInitialEntry()) {
      Browser* browser = chrome::FindBrowserWithTab(contents);
      SetShelfIDForBrowserWindowContents(browser, contents);
    }

    AddWebContentsObserver(contents);
  }
  if (app_service_instance_helper_)
    app_service_instance_helper_->OnTabInserted(contents);
}

void BrowserStatusMonitor::OnTabClosing(content::WebContents* contents) {
  if (!web_app::IsWebAppsCrosapiEnabled()) {
    UpdateAppItemState(contents, true /*remove*/);
    RemoveWebContentsObserver(contents);
  }
  if (app_service_instance_helper_)
    app_service_instance_helper_->OnTabClosing(contents);
}

void BrowserStatusMonitor::OnTabMoved(TabStripModel* tab_strip_model,
                                      content::WebContents* contents) {
  // TODO(crbug.com/40763808): split this into inserted and moved cases.
  OnTabInserted(tab_strip_model, contents);
}

void BrowserStatusMonitor::OnTabNavigationFinished(
    content::WebContents* contents) {
  if (web_app::IsWebAppsCrosapiEnabled()) {
    return;
  }
  UpdateAppItemState(contents, false /*remove*/);
  UpdateBrowserItemState();

  // Navigating may change the ShelfID associated with the WebContents.
  Browser* browser = chrome::FindBrowserWithTab(contents);
  if (browser &&
      browser->tab_strip_model()->GetActiveWebContents() == contents) {
    SetShelfIDForBrowserWindowContents(browser, contents);
  }
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
  if (!web_app::IsWebAppsCrosapiEnabled()) {
    shelf_controller_->SetShelfIDForBrowserWindowContents(browser,
                                                          web_contents);
  }

  if (app_service_instance_helper_) {
    app_service_instance_helper_->OnSetShelfIDForBrowserWindowContents(
        web_contents);
  }
}
