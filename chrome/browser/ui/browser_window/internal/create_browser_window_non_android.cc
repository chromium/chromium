// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"

namespace {

BrowserWindowInterface* CreateAppBrowserWindow(
    BrowserWindowCreateParams create_params) {
  CHECK(create_params.type == BrowserWindowInterface::TYPE_APP ||
        create_params.type == BrowserWindowInterface::TYPE_APP_POPUP)
      << "Unexpected browser type with `app_name`: "
      << static_cast<int>(create_params.type);
  Browser::CreateParams browser_params =
      create_params.type == BrowserWindowInterface::TYPE_APP
          ? Browser::CreateParams::CreateForApp(
                create_params.app_name, create_params.is_trusted_source,
                create_params.initial_bounds, &*create_params.profile,
                create_params.from_user_gesture)
          : Browser::CreateParams::CreateForAppPopup(
                create_params.app_name, create_params.is_trusted_source,
                create_params.initial_bounds, &*create_params.profile,
                create_params.from_user_gesture);

  browser_params.initial_show_state = create_params.initial_show_state;

  return Browser::Create(browser_params);
}

}  // namespace

BrowserWindowInterface* CreateBrowserWindow(
    BrowserWindowCreateParams create_params) {
  if (!create_params.app_name.empty()) {
    return CreateAppBrowserWindow(std::move(create_params));
  }

  Browser::CreateParams browser_params(create_params.type,
                                       &*create_params.profile,
                                       create_params.from_user_gesture);
  browser_params.trusted_source = create_params.is_trusted_source;
  browser_params.initial_bounds = std::move(create_params.initial_bounds);
  browser_params.initial_show_state = create_params.initial_show_state;

  return Browser::Create(browser_params);
}
