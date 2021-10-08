// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_CHROME_URL_WINDOW_MANAGER_OBSERVER_H_
#define CHROME_BROWSER_UI_ASH_CHROME_URL_WINDOW_MANAGER_OBSERVER_H_

class Browser;

class ChromeUrlWindowManagerObserver : public base::CheckedObserver {
 public:
  // Called when a new Chrome URL browser window is created.
  virtual void OnNewChromeUrlWindow(Browser* chrome_url_browser) = 0;
};

#endif  // CHROME_BROWSER_UI_ASH_CHROME_URL_WINDOW_MANAGER_OBSERVER_H_
