// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/browser_apps_tracker.h"

#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/macros.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/ui/ash/shelf/shelf_controller_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"

namespace {

Browser* GetBrowserWithTabStripModel(TabStripModel* tab_strip_model) {
  for (auto* browser : *BrowserList::GetInstance()) {
    if (browser->tab_strip_model() == tab_strip_model)
      return browser;
  }
  return nullptr;
}

std::string GetAppId(content::WebContents* contents) {
  // TODO(crbug.com/1203992): shelf-specific logic doesn't really belong here,
  // replace with more generic implementation to detect apps, and move
  // shelf-specific bits to ChromeShelfController.
  return GetShelfAppIdForWebContents(contents).value_or("");
}

bool IsBrowserVisible(Browser* browser) {
  aura::Window* window = browser->window()->GetNativeWindow();
  return window->IsVisible();
}

bool IsBrowserActive(Browser* browser) {
  return browser->window()->IsActive();
}

bool IsAppVisible(Browser* browser, content::WebContents* contents) {
  return IsBrowserVisible(browser);
}

bool IsAppActive(Browser* browser, content::WebContents* contents) {
  return IsBrowserActive(browser) &&
         browser->tab_strip_model()->GetActiveWebContents() == contents;
}

}  // namespace

// Helper class to notify BrowserAppsTracker when WebContents navigation
// finishes.
class BrowserAppsTracker::WebContentsObserver
    : public content::WebContentsObserver {
 public:
  explicit WebContentsObserver(content::WebContents* contents,
                               BrowserAppsTracker* owner)
      : content::WebContentsObserver(contents), owner_(owner) {}
  WebContentsObserver(const WebContentsObserver&) = delete;
  WebContentsObserver& operator=(const WebContentsObserver&) = delete;
  ~WebContentsObserver() override = default;

  // content::WebContentsObserver
  void DidFinishNavigation(content::NavigationHandle* handle) override {
    // TODO(crbug.com/1229189): Replace this callback with
    // WebContentObserver::PrimaryPageChanged() when fixed.
    if (handle->IsInPrimaryMainFrame() && handle->HasCommitted()) {
      owner_->OnTabNavigationFinished(web_contents());
    }
  }

 private:
  BrowserAppsTracker* owner_;
};

const base::Feature BrowserAppsTracker::kEnabled{
    "EnableBrowserAppsTracker", base::FEATURE_DISABLED_BY_DEFAULT};

BrowserAppsTracker::BrowserAppsTracker(
    apps::AppRegistryCache& app_registry_cache)
    : apps::AppRegistryCache::Observer(&app_registry_cache),
      browser_tab_strip_tracker_(this, nullptr) {
  BrowserList::GetInstance()->AddObserver(this);
}

BrowserAppsTracker::~BrowserAppsTracker() {
  BrowserList::GetInstance()->RemoveObserver(this);
}

void BrowserAppsTracker::Initialize() {
  browser_tab_strip_tracker_.Init();
}

std::set<const BrowserAppInstance*> BrowserAppsTracker::GetAppInstancesByAppId(
    const std::string& app_id) const {
  std::set<const BrowserAppInstance*> result;
  for (const auto& pair : app_instances_) {
    const auto& app_instance = pair.second;
    if (app_instance->app_id == app_id) {
      result.insert(app_instance.get());
    }
  }
  for (const auto& pair : chrome_instances_) {
    const auto& app_instance = pair.second;
    if (app_instance->app_id == app_id) {
      result.insert(app_instance.get());
    }
  }
  return result;
}

bool BrowserAppsTracker::IsAppRunning(const std::string& app_id) const {
  for (const auto& pair : app_instances_) {
    const auto& app_instance = pair.second;
    if (app_instance->app_id == app_id) {
      return true;
    }
  }
  for (const auto& pair : chrome_instances_) {
    const auto& app_instance = pair.second;
    if (app_instance->app_id == app_id) {
      return true;
    }
  }
  return false;
}

const BrowserAppInstance* BrowserAppsTracker::GetAppInstance(
    content::WebContents* contents) const {
  auto it = app_instances_.find(contents);
  return it == app_instances_.end() ? nullptr : it->second.get();
}

const BrowserAppInstance* BrowserAppsTracker::GetAppInstanceByWebContentsId(
    WebContentsId web_contents_id) const {
  for (const auto& pair : app_instances_) {
    const auto& app_instance = pair.second;
    if (app_instance->web_contents_id == web_contents_id) {
      return app_instance.get();
    }
  }
  return nullptr;
}

const BrowserAppInstance* BrowserAppsTracker::GetChromeInstance(
    Browser* browser) const {
  auto it = chrome_instances_.find(browser);
  return it == chrome_instances_.end() ? nullptr : it->second.get();
}

void BrowserAppsTracker::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  Browser* browser = GetBrowserWithTabStripModel(tab_strip_model);
  DCHECK(browser);

  switch (change.type()) {
    case TabStripModelChange::kInserted:
      OnTabStripModelChangeInsert(browser, *change.GetInsert(), selection);
      break;
    case TabStripModelChange::kRemoved:
      OnTabStripModelChangeRemove(browser, *change.GetRemove(), selection);
      break;
    case TabStripModelChange::kReplaced:
      OnTabStripModelChangeReplace(browser, *change.GetReplace());
      break;
    case TabStripModelChange::kMoved:
      // Ignored.
      break;
    case TabStripModelChange::kSelectionOnly:
      OnTabStripModelChangeSelection(browser, selection);
      break;
  }
}

void BrowserAppsTracker::OnWindowVisibilityChanged(aura::Window* window,
                                                   bool visible) {
  DCHECK(window);
  if (auto* browser_view = BrowserView::GetBrowserViewForNativeWindow(window)) {
    OnBrowserWindowUpdated(browser_view->browser());
  }
}

void BrowserAppsTracker::OnBrowserSetLastActive(Browser* browser) {
  OnBrowserWindowUpdated(browser);
}

void BrowserAppsTracker::OnBrowserNoLongerActive(Browser* browser) {
  OnBrowserWindowUpdated(browser);
}

void BrowserAppsTracker::OnAppUpdate(const apps::AppUpdate& update) {
  // Sync app instances for existing tabs.
  for (const auto& pair : browser_to_tab_map_) {
    Browser* browser = pair.first;
    TabStripModel* tab_strip_model = browser->tab_strip_model();
    for (int i = 0; i < tab_strip_model->count(); ++i) {
      content::WebContents* contents = tab_strip_model->GetWebContentsAt(i);
      OnTabUpdated(browser, contents);
    }
  }
}

void BrowserAppsTracker::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  Observe(nullptr);
}

void BrowserAppsTracker::OnTabStripModelChangeInsert(
    Browser* browser,
    const TabStripModelChange::Insert& insert,
    const TabStripSelectionChange& selection) {
  if (selection.old_contents) {
    // A tab got deactivated on insertion.
    OnTabUpdated(browser, selection.old_contents);
  }
  for (const auto& inserted_tab : insert.contents) {
    auto& known_tabs = browser_to_tab_map_[browser];
    if (known_tabs.size() == 0) {
      OnBrowserFirstTabAttached(browser);
      BrowserWindow* window = browser->window();
      if (window && window->GetNativeWindow()) {
        browser_window_observations_.AddObservation(window->GetNativeWindow());
      }
    }
    content::WebContents* contents = inserted_tab.contents;
    known_tabs.insert(contents);
    bool tab_is_new = !base::Contains(webcontents_to_observer_map_, contents);
#if DCHECK_IS_ON()
    if (tab_is_new) {
      DCHECK(!base::Contains(tabs_in_transit_, contents));
    } else {
      // The tab must be in the set of tabs in transit.
      DCHECK(tabs_in_transit_.erase(contents) == 1);
    }
#endif
    if (tab_is_new) {
      webcontents_to_observer_map_[contents] =
          std::make_unique<BrowserAppsTracker::WebContentsObserver>(contents,
                                                                    this);
      OnTabCreated(browser, contents);
    }
    OnTabAttached(browser, contents);
  }
}

void BrowserAppsTracker::OnTabStripModelChangeRemove(
    Browser* browser,
    const TabStripModelChange::Remove& remove,
    const TabStripSelectionChange& selection) {
  for (const auto& removed_tab : remove.contents) {
    auto& known_tabs = browser_to_tab_map_[browser];
    DCHECK(known_tabs.size() > 0);
    content::WebContents* contents = removed_tab.contents;
    known_tabs.erase(contents);
    bool tab_will_be_closed = false;
    switch (removed_tab.remove_reason) {
      case TabStripModelChange::RemoveReason::kDeleted:
      case TabStripModelChange::RemoveReason::kCached:
#if DCHECK_IS_ON()
        DCHECK(!base::Contains(tabs_in_transit_, contents));
#endif
        tab_will_be_closed = true;
        break;
      case TabStripModelChange::RemoveReason::kInsertedIntoOtherTabStrip:
        // The tab will be reinserted immediately into another browser, so
        // this event is ignored.
        if (browser->is_type_devtools()) {
          // TODO(crbug.com/1221967): when a dev tools window is docked, and
          // its WebContents is removed, it will not be reinserted into
          // another tab strip, so it should be treated as closed.
          tab_will_be_closed = true;
        } else {
          // The tab must not be already in the set of tabs in transit.
#if DCHECK_IS_ON()
          DCHECK(tabs_in_transit_.insert(contents).second);
#endif
        }
        break;
    }
    if (tab_will_be_closed) {
      OnTabClosing(browser, contents);
    }
    if (tab_will_be_closed) {
      DCHECK(base::Contains(webcontents_to_observer_map_, contents));
      webcontents_to_observer_map_.erase(contents);
    }
    if (known_tabs.empty()) {
      BrowserWindow* window = browser->window();
      DCHECK(window && window->GetNativeWindow());
      browser_window_observations_.RemoveObservation(window->GetNativeWindow());
      OnBrowserLastTabDetached(browser);
      browser_to_tab_map_.erase(browser);
    }
  }
  if (selection.new_contents) {
    // A tab got activated on removal.
    OnTabUpdated(browser, selection.new_contents);
  }
}

void BrowserAppsTracker::OnTabStripModelChangeReplace(
    Browser* browser,
    const TabStripModelChange::Replace& replace) {
  // Simulate closing the old tab and opening a new tab.
  OnTabClosing(browser, replace.old_contents);
  OnTabCreated(browser, replace.new_contents);
  OnTabAttached(browser, replace.new_contents);
}

void BrowserAppsTracker::OnTabStripModelChangeSelection(
    Browser* browser,
    const TabStripSelectionChange& selection) {
  if (!selection.active_tab_changed()) {
    return;
  }
  if (selection.new_contents) {
    OnTabUpdated(browser, selection.new_contents);
  }
  if (selection.old_contents) {
    OnTabUpdated(browser, selection.old_contents);
  }
}

void BrowserAppsTracker::OnBrowserFirstTabAttached(Browser* browser) {
  if (browser->is_type_normal()) {
    CreateChromeInstance(browser);
  }
}

void BrowserAppsTracker::OnBrowserLastTabDetached(Browser* browser) {
  RemoveChromeInstanceIfExists(browser);
}

void BrowserAppsTracker::OnTabCreated(Browser* browser,
                                      content::WebContents* contents) {
  std::string app_id = GetAppId(contents);
  if (!app_id.empty()) {
    CreateAppInstance(std::move(app_id), browser, contents);
  }
}

void BrowserAppsTracker::OnTabAttached(Browser* browser,
                                       content::WebContents* contents) {
  auto it = app_instances_.find(contents);
  if (it != app_instances_.end()) {
    auto& app_instance = it->second;
    MaybeUpdateAppInstance(*app_instance, browser);
  }
}

void BrowserAppsTracker::OnTabUpdated(Browser* browser,
                                      content::WebContents* contents) {
  std::string new_app_id = GetAppId(contents);
  auto it = app_instances_.find(contents);
  if (it != app_instances_.end()) {
    auto& app_instance = it->second;
    if (app_instance->app_id != new_app_id) {
      // If app ID changed on navigation, remove the old app.
      RemoveAppInstanceIfExists(contents);
      // Add the new app instance, if navigated to another app.
      if (!new_app_id.empty()) {
        CreateAppInstance(std::move(new_app_id), browser, contents);
      }
    } else {
      // App ID did not change, but other attributes may have.
      MaybeUpdateAppInstance(*app_instance, browser);
    }
  } else if (!new_app_id.empty()) {
    // Tab previously had no app ID, but navigated to a URL that does.
    CreateAppInstance(std::move(new_app_id), browser, contents);
  } else {
    // Tab without an app has changed, we don't care about it.
  }
}

void BrowserAppsTracker::OnTabClosing(Browser* browser,
                                      content::WebContents* contents) {
  RemoveAppInstanceIfExists(contents);
}

void BrowserAppsTracker::OnTabNavigationFinished(
    content::WebContents* contents) {
  Browser* browser = chrome::FindBrowserWithWebContents(contents);
  if (browser) {
    OnTabUpdated(browser, contents);
  }
}

void BrowserAppsTracker::OnBrowserWindowUpdated(Browser* browser) {
  // We only want to send window events for the browsers we track to avoid
  // sending window events before a "browser added" event.
  if (!base::Contains(browser_to_tab_map_, browser)) {
    return;
  }
  auto it = chrome_instances_.find(browser);
  if (it != chrome_instances_.end()) {
    MaybeUpdateChromeInstance(*it->second);
  }

  TabStripModel* tab_strip_model = browser->tab_strip_model();
  for (int i = 0; i < tab_strip_model->count(); i++) {
    content::WebContents* contents = tab_strip_model->GetWebContentsAt(i);
    OnTabUpdated(browser, contents);
  }
}

void BrowserAppsTracker::CreateAppInstance(std::string app_id,
                                           Browser* browser,
                                           content::WebContents* contents) {
  CreateInstance(app_instances_, contents,
                 base::WrapUnique(new BrowserAppInstance{
                     std::move(app_id),
                     browser,
                     contents,
                     ++last_web_contents_id_,
                     IsAppVisible(browser, contents),
                     IsAppActive(browser, contents),
                 }));
}

void BrowserAppsTracker::MaybeUpdateAppInstance(BrowserAppInstance& instance,
                                                Browser* browser) {
  MaybeUpdateInstance(instance, browser);
}

void BrowserAppsTracker::RemoveAppInstanceIfExists(
    content::WebContents* contents) {
  RemoveInstanceIfExists(app_instances_, contents);
}

void BrowserAppsTracker::CreateChromeInstance(Browser* browser) {
  CreateInstance(chrome_instances_, browser,
                 base::WrapUnique(new BrowserAppInstance{
                     extension_misc::kChromeAppId,
                     browser,
                     nullptr /* contents */,
                     0 /* web_contents_id */,
                     IsBrowserVisible(browser),
                     IsBrowserActive(browser),
                 }));
}

void BrowserAppsTracker::MaybeUpdateChromeInstance(
    BrowserAppInstance& instance) {
  // Browser does not change for Chrome instances.
  MaybeUpdateInstance(instance, instance.browser);
}

void BrowserAppsTracker::RemoveChromeInstanceIfExists(Browser* browser) {
  RemoveInstanceIfExists(chrome_instances_, browser);
}

template <typename KeyT>
void BrowserAppsTracker::CreateInstance(
    std::map<KeyT, std::unique_ptr<BrowserAppInstance>>& instances,
    const KeyT& key,
    std::unique_ptr<BrowserAppInstance> instance_ptr) {
  DCHECK(!base::Contains(instances, key));
  auto it = instances.insert(std::make_pair(key, std::move(instance_ptr)));
  auto& inserted_instance_ptr = it.first->second;
  for (auto& observer : observers_) {
    observer.OnBrowserAppAdded(*inserted_instance_ptr);
  }
}

void BrowserAppsTracker::MaybeUpdateInstance(BrowserAppInstance& instance,
                                             Browser* browser) {
  DCHECK(browser);
  bool visible;
  bool active;
  if (instance.web_contents) {
    visible = IsAppVisible(browser, instance.web_contents);
    active = IsAppActive(browser, instance.web_contents);
  } else {
    visible = IsBrowserVisible(browser);
    active = IsBrowserActive(browser);
  }
  if (instance.browser == browser && instance.visible == visible &&
      instance.active == active) {
    return;
  }
  instance.browser = browser;
  instance.visible = visible;
  instance.active = active;

  for (auto& observer : observers_) {
    observer.OnBrowserAppUpdated(instance);
  }
}

template <typename KeyT>
void BrowserAppsTracker::RemoveInstanceIfExists(
    std::map<KeyT, std::unique_ptr<BrowserAppInstance>>& instances,
    const KeyT& key) {
  auto it = instances.find(key);
  if (it == instances.end()) {
    return;
  }
  auto app_instance = std::move(it->second);
  instances.erase(it);
  for (auto& observer : observers_) {
    observer.OnBrowserAppRemoved(*app_instance);
  }
}
