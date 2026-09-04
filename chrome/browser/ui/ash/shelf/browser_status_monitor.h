// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELF_BROWSER_STATUS_MONITOR_H_
#define CHROME_BROWSER_UI_ASH_SHELF_BROWSER_STATUS_MONITOR_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>

#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/ash/shelf/app_service/app_service_instance_registry_helper.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chromeos/ash/components/browser_delegate/browser_controller.h"

namespace ash {
class BrowserDelegate;
}

// BrowserStatusMonitor monitors creation/deletion of browsers and their
// tabs to keep the shelf representation up to date as the active tab
// changes.
class BrowserStatusMonitor : public ash::BrowserController::Observer,
                             public ash::BrowserController::TabObserver {
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

  // ash::BrowserController::Observer overrides:
  void OnBrowserCreated(ash::BrowserDelegate* browser) override;
  void OnBrowserClosed(ash::BrowserDelegate* browser) override;

  // ash::BrowserController::TabObserver overrides:
  void OnTabInserted(ash::BrowserDelegate* browser,
                     content::WebContents* contents) override;
  void OnTabRemoved(ash::BrowserDelegate* browser,
                    content::WebContents* contents,
                    bool will_delete) override;
  void OnTabReplaced(ash::BrowserDelegate* browser,
                     content::WebContents* old_contents,
                     content::WebContents* new_contents) override;
  void OnActiveWebContentsChanged(ash::BrowserDelegate* browser,
                                  content::WebContents* old_contents,
                                  content::WebContents* new_contents) override;

 private:
  // Add a windowed browser-based app to the shelf.
  void AddAppBrowserToShelf(ash::BrowserDelegate* browser);

  // Remove a windowed browser-based app from the shelf.
  void RemoveAppBrowserFromShelf(ash::BrowserDelegate* browser);

  // Check if an application is currently in the shelf by browser or app id.
  bool IsAppBrowserInShelf(ash::BrowserDelegate* browser);
  bool IsAppBrowserInShelfWithAppId(const std::string& app_id);

  class LocalWebContentsObserver;

  // Called by LocalWebContentsObserver.
  void OnTabNavigationFinished(content::WebContents* contents);

  // Create LocalWebContentsObserver for |contents|.
  void AddWebContentsObserver(content::WebContents* contents);

  // Remove LocalWebContentsObserver for |contents|.
  void RemoveWebContentsObserver(content::WebContents* contents);

  // Sets the shelf id for browsers represented by the browser shortcut item.
  void SetShelfIDForBrowserWindowContents(ash::BrowserDelegate* browser,
                                          content::WebContents* web_contents);

  raw_ptr<ChromeShelfController> shelf_controller_;
  std::map<raw_ptr<ash::BrowserDelegate>, std::string> browser_to_app_id_map_;
  std::map<content::WebContents*, std::unique_ptr<LocalWebContentsObserver>>
      webcontents_to_observer_map_;
  base::ScopedObservation<ash::BrowserController,
                          ash::BrowserController::Observer>
      browser_observation_{this};
  base::ScopedObservation<ash::BrowserController,
                          ash::BrowserController::TabObserver>
      tab_observation_{this};
  bool initialized_ = false;
  raw_ptr<AppServiceInstanceRegistryHelper> app_service_instance_helper_ =
      nullptr;
};

#endif  // CHROME_BROWSER_UI_ASH_SHELF_BROWSER_STATUS_MONITOR_H_
