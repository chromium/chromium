// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/browser_status_monitor.h"

#include <memory>

#include "ash/public/cpp/shelf_types.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/shelf/app_service/app_service_app_window_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller_util.h"
#include "chrome/browser/ui/ash/shelf/shelf_spinner_controller.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chromeos/ash/components/browser_delegate/browser_controller.h"
#include "chromeos/ash/components/browser_delegate/browser_delegate.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

namespace {

// Checks if a given browser is running a windowed app. It will return true for
// web apps, hosted apps, and packaged V1 apps.
bool IsAppBrowser(const ash::BrowserDelegate* browser) {
  const ash::BrowserType browser_type = browser->GetType();
  if (browser_type != ash::BrowserType::kApp &&
      browser_type != ash::BrowserType::kAppPopup) {
    return false;
  }

  return browser->GetAppId().has_value();
}

// Returns the index of the given web contents in the browser, or std::nullopt
// if not found.
std::optional<size_t> GetIndexOfWebContents(
    const ash::BrowserDelegate& browser,
    const content::WebContents* contents) {
  if (contents) {
    for (size_t i = 0; i < browser.GetWebContentsCount(); ++i) {
      if (browser.GetWebContentsAt(i) == contents) {
        return i;
      }
    }
  }
  return std::nullopt;
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
    : shelf_controller_(shelf_controller) {
  DCHECK(shelf_controller_);

  app_service_instance_helper_ =
      shelf_controller->app_service_app_window_controller()
          ->app_service_instance_helper();
  DCHECK(app_service_instance_helper_);
}

BrowserStatusMonitor::~BrowserStatusMonitor() {
  DCHECK(initialized_);

  browser_observation_.Reset();
  tab_observation_.Reset();

  // Simulate OnBrowserClosed() for all Browsers.
  ash::BrowserController::GetInstance()->ForEachBrowser(
      ash::BrowserController::BrowserOrder::kAscendingActivationTime,
      [&](ash::BrowserDelegate& browser) {
        OnBrowserClosed(&browser);
        return ash::BrowserController::kContinueIteration;
      });
}

void BrowserStatusMonitor::Initialize() {
  DCHECK(!initialized_);
  initialized_ = true;

  // Simulate OnBrowserCreated() and initial tab insertions for all existing
  // Browsers.
  ash::BrowserController::GetInstance()->ForEachBrowser(
      ash::BrowserController::BrowserOrder::kAscendingActivationTime,
      [&](ash::BrowserDelegate& browser) {
        OnBrowserCreated(&browser);
        for (size_t i = 0; i < browser.GetWebContentsCount(); ++i) {
          if (content::WebContents* contents = browser.GetWebContentsAt(i)) {
            OnTabInserted(&browser, contents);
          }
        }
        return ash::BrowserController::kContinueIteration;
      });

  browser_observation_.Observe(ash::BrowserController::GetInstance());
  tab_observation_.Observe(ash::BrowserController::GetInstance());
}

void BrowserStatusMonitor::ActiveUserChanged(const std::string& user_email) {
  // When the active profile changes, all windowed and tabbed apps owned by the
  // newly selected profile are added to the shelf, and the ones owned by other
  // profiles are removed.
  ash::BrowserController::GetInstance()->ForEachBrowser(
      ash::BrowserController::BrowserOrder::kAscendingActivationTime,
      [&](ash::BrowserDelegate& browser) {
        const bool owned = multi_user_util::IsProfileFromActiveUser(
            browser.GetBrowser().GetProfile());

        const ash::BrowserType browser_type = browser.GetType();
        if (browser_type == ash::BrowserType::kApp ||
            browser_type == ash::BrowserType::kAppPopup) {
          // Add windowed apps owned by the current profile, and remove the one
          // owned by other profiles.
          const bool app_in_shelf = IsAppBrowserInShelf(&browser);
          content::WebContents* const active_web_contents =
              browser.GetActiveWebContents();

          if (owned && !app_in_shelf) {
            // Adding an app to the shelf consists of two actions: add the
            // browser (shelf item) and add the content (shelf item status).
            AddAppBrowserToShelf(&browser);
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
            RemoveAppBrowserFromShelf(&browser);
          }

        } else if (browser_type == ash::BrowserType::kNormal) {
          // Add tabbed apps owned by the current profile, and remove the ones
          // owned by other profiles.
          for (size_t i = 0; i < browser.GetWebContentsCount(); ++i) {
            shelf_controller_->UpdateAppState(browser.GetWebContentsAt(i),
                                              !owned /*remove*/);
          }
        }
        return ash::BrowserController::kContinueIteration;
      });

  // Update the browser state since some of the additions/removals above might
  // have had an impact on the browser item state.
  UpdateBrowserItemState();
}

void BrowserStatusMonitor::UpdateAppItemState(content::WebContents* contents,
                                              bool remove) {
  DCHECK(contents);
  DCHECK(initialized_);
  // It is possible to come here from Browser::SwapTabContent where the contents
  // cannot be associated with a browser. A removal however should be properly
  // processed.
  ash::BrowserDelegate* browser =
      ash::BrowserController::GetInstance()->GetBrowserForTab(contents);
  if (remove || (browser && multi_user_util::IsProfileFromActiveUser(
                                browser->GetBrowser().GetProfile()))) {
    shelf_controller_->UpdateAppState(contents, remove);
  }
}

void BrowserStatusMonitor::UpdateBrowserItemState() {
  DCHECK(initialized_);
  shelf_controller_->UpdateBrowserItemState();
}

void BrowserStatusMonitor::OnBrowserCreated(ash::BrowserDelegate* browser) {
  DCHECK(initialized_);

  if (IsAppBrowser(browser) && multi_user_util::IsProfileFromActiveUser(
                                   browser->GetBrowser().GetProfile())) {
    AddAppBrowserToShelf(browser);
  }
}

void BrowserStatusMonitor::OnBrowserClosed(ash::BrowserDelegate* browser) {
  DCHECK(initialized_);

  RemoveAppBrowserFromShelf(browser);
  UpdateBrowserItemState();

  if (app_service_instance_helper_) {
    app_service_instance_helper_->OnBrowserRemoved();
  }
}

void BrowserStatusMonitor::OnTabInserted(ash::BrowserDelegate* browser,
                                         content::WebContents* contents) {
  UpdateAppItemState(contents, false /*remove*/);
  // If the visible navigation entry is the initial entry, wait until a
  // navigation status changes before setting the browser window Shelf ID
  // (done by the web contents observer added by AddWebContentsObserver()).
  if (browser->GetActiveWebContents() == contents &&
      !contents->GetController().GetVisibleEntry()->IsInitialEntry()) {
    SetShelfIDForBrowserWindowContents(browser, contents);
  }

  AddWebContentsObserver(contents);

  if (app_service_instance_helper_) {
    app_service_instance_helper_->OnTabInserted(contents);
  }

  UpdateBrowserItemState();
}

void BrowserStatusMonitor::OnTabRemoved(ash::BrowserDelegate* browser,
                                        content::WebContents* contents,
                                        bool will_delete) {
  if (will_delete || browser->GetType() == ash::BrowserType::kDevTools) {
    // TODO(crbug.com/40773744): when a dev tools window is docked, and
    // its WebContents is removed, it will not be reinserted into
    // another tab strip, so it should be treated as closed.
    UpdateAppItemState(contents, true /*remove*/);
    RemoveWebContentsObserver(contents);

    if (app_service_instance_helper_) {
      app_service_instance_helper_->OnTabClosing(contents);
    }
  }
}

void BrowserStatusMonitor::OnTabReplaced(ash::BrowserDelegate* browser,
                                         content::WebContents* old_contents,
                                         content::WebContents* new_contents) {
  DCHECK(old_contents);
  DCHECK(new_contents);

  UpdateAppItemState(old_contents, true /*remove*/);
  RemoveWebContentsObserver(old_contents);

  UpdateAppItemState(new_contents, false /*remove*/);
  UpdateBrowserItemState();

  if (IsAppBrowserInShelf(browser) && multi_user_util::IsProfileFromActiveUser(
                                          browser->GetBrowser().GetProfile())) {
    shelf_controller_->SetAppStatus(browser->GetAppId().value_or(std::string()),
                                    ash::STATUS_RUNNING);
  }

  if (browser->GetActiveWebContents() == new_contents) {
    SetShelfIDForBrowserWindowContents(browser, new_contents);
  }

  AddWebContentsObserver(new_contents);

  if (app_service_instance_helper_) {
    app_service_instance_helper_->OnTabReplaced(old_contents, new_contents);
  }
}

void BrowserStatusMonitor::OnActiveWebContentsChanged(
    ash::BrowserDelegate* browser,
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  // Use |new_contents|. |old_contents| could be nullptr.
  DCHECK(new_contents);

  // Update immediately on a tab change.
  if (old_contents &&
      GetIndexOfWebContents(*browser, old_contents).has_value()) {
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

void BrowserStatusMonitor::AddAppBrowserToShelf(ash::BrowserDelegate* browser) {
  DCHECK(IsAppBrowser(browser));
  DCHECK(initialized_);

  const std::string app_id = *browser->GetAppId();
  DCHECK(!app_id.empty());
  if (!IsAppBrowserInShelfWithAppId(app_id)) {
    if (auto* chrome_controller = ChromeShelfController::instance()) {
      chrome_controller->GetShelfSpinnerController()->CloseSpinner(app_id);
    }
    shelf_controller_->SetAppStatus(app_id, ash::STATUS_RUNNING);
  }
  browser_to_app_id_map_[browser] = app_id;
}

void BrowserStatusMonitor::RemoveAppBrowserFromShelf(
    ash::BrowserDelegate* browser) {
  DCHECK(initialized_);
  auto iter = browser_to_app_id_map_.find(browser);
  if (iter != browser_to_app_id_map_.end()) {
    DCHECK(IsAppBrowser(browser));
    const std::string app_id = iter->second;
    browser_to_app_id_map_.erase(iter);
    if (!IsAppBrowserInShelfWithAppId(app_id)) {
      shelf_controller_->SetAppStatus(app_id, ash::STATUS_CLOSED);
    }
  }
}

bool BrowserStatusMonitor::IsAppBrowserInShelf(ash::BrowserDelegate* browser) {
  return browser_to_app_id_map_.find(browser) != browser_to_app_id_map_.end();
}

bool BrowserStatusMonitor::IsAppBrowserInShelfWithAppId(
    const std::string& app_id) {
  for (const auto& iter : browser_to_app_id_map_) {
    if (iter.second == app_id) {
      return true;
    }
  }
  return false;
}

void BrowserStatusMonitor::OnTabNavigationFinished(
    content::WebContents* contents) {
  UpdateAppItemState(contents, false /*remove*/);
  UpdateBrowserItemState();

  // Navigating may change the ShelfID associated with the WebContents.
  ash::BrowserDelegate* browser =
      ash::BrowserController::GetInstance()->GetBrowserForTab(contents);
  if (browser && browser->GetActiveWebContents() == contents) {
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
    ash::BrowserDelegate* browser,
    content::WebContents* web_contents) {
  shelf_controller_->SetShelfIDForBrowserWindowContents(browser, web_contents);

  if (app_service_instance_helper_) {
    app_service_instance_helper_->OnSetShelfIDForBrowserWindowContents(
        web_contents);
  }
}
