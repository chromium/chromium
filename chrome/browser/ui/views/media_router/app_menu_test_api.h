// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_APP_MENU_TEST_API_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_APP_MENU_TEST_API_H_

#include <memory>

class Browser;

namespace test {

class AppMenuTestApi {
 public:
  static std::unique_ptr<AppMenuTestApi> Create(Browser* browser);

  AppMenuTestApi() = default;
  virtual ~AppMenuTestApi() = default;

  // Note: This is not a general-purpose API for testing the app menu;
  // ShowMenu() and IsMenuShowing() may not *actually* show the menu or return
  // its true status. ExecuteCommand() may not dispatch commands the same way
  // the real menu would.
  virtual bool IsMenuShowing() = 0;
  virtual void ShowMenu() = 0;
  virtual void ExecuteCommand(int command) = 0;
};

}  // namespace test

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_APP_MENU_TEST_API_H_
