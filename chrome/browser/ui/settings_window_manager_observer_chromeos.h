// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SETTINGS_WINDOW_MANAGER_OBSERVER_CHROMEOS_H_
#define CHROME_BROWSER_UI_SETTINGS_WINDOW_MANAGER_OBSERVER_CHROMEOS_H_

class Browser;

namespace chrome {

class SettingsWindowManagerObserver {
 public:
  // Called when a new settings browser window is created.
  virtual void OnNewSettingsWindow(Browser* settings_browser) = 0;

 protected:
  virtual ~SettingsWindowManagerObserver() {}
};

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_SETTINGS_WINDOW_MANAGER_OBSERVER_CHROMEOS_H_
