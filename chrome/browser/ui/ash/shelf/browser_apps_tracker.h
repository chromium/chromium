// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELF_BROWSER_APPS_TRACKER_H_
#define CHROME_BROWSER_UI_ASH_SHELF_BROWSER_APPS_TRACKER_H_

#include <map>
#include <memory>
#include <set>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/observer_list.h"
#include "base/scoped_multi_source_observation.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/shelf/browser_app_status_observer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_tab_strip_tracker.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"

class Browser;

// BrowserAppsTracker monitors changes to Browsers, TabStripModels and browsers'
// native windows to maintain a list of running apps and notify its registered
// observers of any changes:
// - apps running in WebContents (web apps, hosted apps, V1 packaged apps)
// - browser instances (registered with app ID |extension_misc::kChromeAppId|).
class BrowserAppsTracker : public TabStripModelObserver,
                           public aura::WindowObserver,
                           public apps::AppRegistryCache::Observer,
                           public BrowserListObserver {
 public:
  static const base::Feature kEnabled;

  explicit BrowserAppsTracker(apps::AppRegistryCache& app_registry_cache);
  ~BrowserAppsTracker() override;
  BrowserAppsTracker(const BrowserAppsTracker&) = delete;
  BrowserAppsTracker& operator=(const BrowserAppsTracker&) = delete;

  // Causes BrowserAppStatusObserver events to fire for all existing browsers.
  void Initialize();

  // Get all instances by app ID. Returns a set of unowned pointers.
  std::set<const BrowserAppInstance*> GetAppInstancesByAppId(
      const std::string& app_id) const;

  // Checks if at least one instance of the app is running.
  bool IsAppRunning(const std::string& app_id) const;

  // Get app instance running in a |contents|. Returns null if no app is found.
  const BrowserAppInstance* GetAppInstance(
      content::WebContents* contents) const;

  // Get app instance running in a |contents|. Returns null if no app is found.
  const BrowserAppInstance* GetAppInstanceByWebContentsId(
      WebContentsId web_contents_id) const;

  // Get Chrome instance running in |browser|. Returns null if not found.
  const BrowserAppInstance* GetChromeInstance(Browser* browser) const;

  void AddObserver(BrowserAppStatusObserver* observer) {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(BrowserAppStatusObserver* observer) {
    observers_.RemoveObserver(observer);
  }

  // TabStripModelObserver overrides:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // aura::WindowObserver overrides:
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;

  // BrowserListObserver overrides:
  void OnBrowserSetLastActive(Browser* browser) override;
  void OnBrowserNoLongerActive(Browser* browser) override;

  // apps::AppRegistryCache::Observer:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

 private:
  class WebContentsObserver;

  // Called by TabStripModelChanged().
  void OnTabStripModelChangeInsert(Browser* browser,
                                   const TabStripModelChange::Insert& insert,
                                   const TabStripSelectionChange& selection);
  void OnTabStripModelChangeRemove(Browser* browser,
                                   const TabStripModelChange::Remove& remove,
                                   const TabStripSelectionChange& selection);
  void OnTabStripModelChangeReplace(
      Browser* browser,
      const TabStripModelChange::Replace& replace);
  void OnTabStripModelChangeSelection(Browser* browser,
                                      const TabStripSelectionChange& selection);

  // Called by OnTabStripModelChange* functions.
  void OnBrowserFirstTabAttached(Browser* browser);
  void OnBrowserLastTabDetached(Browser* browser);
  void OnTabCreated(Browser* browser, content::WebContents* contents);
  void OnTabAttached(Browser* browser, content::WebContents* contents);
  void OnTabUpdated(Browser* browser, content::WebContents* contents);
  void OnTabClosing(Browser* browser, content::WebContents* contents);

  // Called by |BrowserAppsTracker::WebContentsObserver|.
  void OnTabNavigationFinished(content::WebContents* contents);

  // Called on browser window changes. Sends update events for all open tabs.
  void OnBrowserWindowUpdated(Browser* browser);

  // Creates an app instance for the app running in |WebContents|. Handles both
  // apps in tabs and windows.
  void CreateAppInstance(std::string app_id,
                         Browser* browser,
                         content::WebContents* contents);

  // Updates the app instance with the new attributes and notifies observers, if
  // it was updated.
  void MaybeUpdateAppInstance(BrowserAppInstance& instance, Browser* browser);

  // Removes the app instance, if it exists, and notifies observers.
  void RemoveAppInstanceIfExists(content::WebContents* contents);

  // Creates an app instance for a Chrome browser window.
  void CreateChromeInstance(Browser* browser);

  // Updates the browser instance with the new attributes and notifies
  // observers, if it was updated.
  void MaybeUpdateChromeInstance(BrowserAppInstance& instance);

  // Removes the browser instance, if it exists, and notifies observers.
  void RemoveChromeInstanceIfExists(Browser* browser);

  template <typename KeyT>
  void CreateInstance(
      std::map<KeyT, std::unique_ptr<BrowserAppInstance>>& instances,
      const KeyT& key,
      std::unique_ptr<BrowserAppInstance> instance);

  // Updates the instance (app or browser) with the new attributes and notifies
  // observers, if it was updated.
  void MaybeUpdateInstance(BrowserAppInstance& instance, Browser* browser);

  // Removes the instance given a map (app or browser), if it exists, and
  // notifies observers.
  template <typename KeyT>
  void RemoveInstanceIfExists(
      std::map<KeyT, std::unique_ptr<BrowserAppInstance>>& instances,
      const KeyT& key);

  std::map<content::WebContents*, std::unique_ptr<WebContentsObserver>>
      webcontents_to_observer_map_;

  // Keep track of known tabs per browser as represented by TabStripModel events
  // so we know when the first tab is inserted and the last tab is removed per
  // browser.
  std::map<Browser*, std::set<content::WebContents*>> browser_to_tab_map_;

  // A set of observed browser windows.
  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      browser_window_observations_{this};

  BrowserTabStripTracker browser_tab_strip_tracker_;

#if DCHECK_IS_ON()
  // Tabs that are removed from one browser and are getting reinserted into
  // another.
  std::set<content::WebContents*> tabs_in_transit_;
#endif

  // A map of all apps running in either tabs or windows.
  std::map<content::WebContents*, std::unique_ptr<BrowserAppInstance>>
      app_instances_;

  // A map of Chrome browser windows.
  std::map<Browser*, std::unique_ptr<BrowserAppInstance>> chrome_instances_;

  base::ObserverList<BrowserAppStatusObserver, true>::Unchecked observers_;

  WebContentsId last_web_contents_id_{0};
};

#endif  // CHROME_BROWSER_UI_ASH_SHELF_BROWSER_APPS_TRACKER_H_
