// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELF_BROWSER_STATUS_MONITOR_H_
#define CHROME_BROWSER_UI_ASH_SHELF_BROWSER_STATUS_MONITOR_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/ash/shelf/app_service/app_service_instance_registry_helper.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_tab_strip_tracker.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"

class Browser;

// BrowserStatusMonitor monitors creation/deletion of Browser and its
// TabStripModel to keep the shelf representation up to date as the
// active tab changes.
class BrowserStatusMonitor : public BrowserListObserver,
                             public TabStripModelObserver {
 public:
  explicit BrowserStatusMonitor(ChromeShelfController* shelf_controller);

  BrowserStatusMonitor(const BrowserStatusMonitor&) = delete;
  BrowserStatusMonitor& operator=(const BrowserStatusMonitor&) = delete;

  ~BrowserStatusMonitor() override;

  // Do the initialization work. Note: the init phase is separate from
  // construction because this function will make callbacks to
  // ChromeShelfController and ChromeShelfController creates an instance of this
  // class in its own constructor and may not be fully initialized yet.
  void Initialize();

  // A function which gets called when the current user has changed.
  // Note that this function is called by the ChromeShelfController to be
  // able to do the activation in a proper order - rather then setting an
  // observer.
  void ActiveUserChanged(const std::string& user_email);

  // A shortcut to call the ChromeShelfController's UpdateAppState().
  void UpdateAppItemState(content::WebContents* contents, bool remove);

  // A shortcut to call the BrowserShortcutShelfItemController's
  // UpdateBrowserItemState().
  void UpdateBrowserItemState();

  // BrowserListObserver overrides:
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

  // TabStripModelObserver overrides:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

 private:
  // Add a windowed browser-based app to the shelf.
  void AddAppBrowserToShelf(Browser* browser);

  // Remove a windowed browser-based app from the shelf.
  void RemoveAppBrowserFromShelf(Browser* browser);

  // Check if an application is currently in the shelf by browser or app id.
  bool IsAppBrowserInShelf(Browser* browser);
  bool IsAppBrowserInShelfWithAppId(const std::string& app_id);

  class LocalWebContentsObserver;

  // Called by TabStripModelChanged()
  void OnActiveTabChanged(content::WebContents* old_contents,
                          content::WebContents* new_contents);
  void OnTabReplaced(TabStripModel* tab_strip_model,
                     content::WebContents* old_contents,
                     content::WebContents* new_contents);
  void OnTabInserted(TabStripModel* tab_strip_model,
                     content::WebContents* contents);
  void OnTabClosing(content::WebContents* contents);
  // Tab is moved between browsers
  void OnTabMoved(TabStripModel* tab_strip_model,
                  content::WebContents* contents);

  // Called by LocalWebContentsObserver.
  void OnTabNavigationFinished(content::WebContents* contents);

  // Create LocalWebContentsObserver for |contents|.
  void AddWebContentsObserver(content::WebContents* contents);

  // Remove LocalWebContentsObserver for |contents|.
  void RemoveWebContentsObserver(content::WebContents* contents);

  // Sets the shelf id for browsers represented by the browser shortcut item.
  void SetShelfIDForBrowserWindowContents(Browser* browser,
                                          content::WebContents* web_contents);

  raw_ptr<ChromeShelfController> shelf_controller_;

  std::map<Browser*, std::string> browser_to_app_id_map_;
  std::map<content::WebContents*, std::unique_ptr<LocalWebContentsObserver>>
      webcontents_to_observer_map_;

  BrowserTabStripTracker browser_tab_strip_tracker_;
  bool initialized_ = false;

  raw_ptr<AppServiceInstanceRegistryHelper> app_service_instance_helper_ =
      nullptr;

#if DCHECK_IS_ON()
  // Browsers for which OnBrowserAdded() was called, but not OnBrowserRemoved().
  // Used to validate that OnBrowserAdded() is invoked before
  // OnTabStripModelChanged().
  std::set<raw_ptr<Browser, SetExperimental>> known_browsers_;
  // Tabs that are removed from one browser and are getting reinserted into
  // another.
  std::set<raw_ptr<content::WebContents, DanglingUntriaged>> tabs_in_transit_;
#endif
};

#endif  // CHROME_BROWSER_UI_ASH_SHELF_BROWSER_STATUS_MONITOR_H_
