// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELF_CHROME_URL_WINDOW_OBSERVER_H_
#define CHROME_BROWSER_UI_ASH_SHELF_CHROME_URL_WINDOW_OBSERVER_H_

#include <memory>

#include "base/macros.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/ash/chrome_url_window_manager.h"
#include "chrome/browser/ui/ash/chrome_url_window_manager_observer.h"
#include "ui/aura/window_tracker.h"

class ChromeUrlWindowManager;

// Sets the window title and shelf item properties for ChromeUrl windows.
// ChromeUrl windows are not handled by BrowserShortcutShelfItemController.
class ChromeUrlWindowObserver : public ChromeUrlWindowManagerObserver {
 public:
  explicit ChromeUrlWindowObserver(ChromeUrlWindowManager* window_manager);
  ChromeUrlWindowObserver(const ChromeUrlWindowObserver&) = delete;
  ChromeUrlWindowObserver& operator=(const ChromeUrlWindowObserver&) = delete;

  ~ChromeUrlWindowObserver() override;

  // chrome::ChromeUrlWindowManagerObserver:
  void OnNewChromeUrlWindow(Browser* chrome_url_browser) override;

 private:
  // A helper class which keeps the window title up to date.
  std::unique_ptr<aura::WindowTracker> aura_window_tracker_;

  base::ScopedObservation<ChromeUrlWindowManager,
                          ChromeUrlWindowManagerObserver>
      observation_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_SHELF_CHROME_URL_WINDOW_OBSERVER_H_
