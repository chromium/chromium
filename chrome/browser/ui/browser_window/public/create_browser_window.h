// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_CREATE_BROWSER_WINDOW_H_
#define CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_CREATE_BROWSER_WINDOW_H_

#include <string>

#include "base/memory/raw_ref.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "ui/base/mojom/window_show_state.mojom.h"

// Parameters used when creating a new browser window.
struct BrowserWindowCreateParams {
  BrowserWindowCreateParams(BrowserWindowInterface::Type type,
                            Profile& profile,
                            bool from_user_gesture);
  BrowserWindowCreateParams(Profile& profile, bool from_user_gesture);
  BrowserWindowCreateParams(BrowserWindowCreateParams&&);
  BrowserWindowCreateParams(const BrowserWindowCreateParams&) = delete;
  BrowserWindowCreateParams& operator=(const BrowserWindowCreateParams&) =
      delete;
  BrowserWindowCreateParams& operator=(BrowserWindowCreateParams&&);
  ~BrowserWindowCreateParams();

  // The type of browser window to create.
  // See BrowserWindowInterface::Type for more details.
  BrowserWindowInterface::Type type = BrowserWindowInterface::TYPE_NORMAL;

  // Whether the browser was created by a user gesture.
  bool from_user_gesture = false;

  // The profile to be associated with the browser window.
  raw_ref<Profile> profile;

  // The initial bounds of the window. If unsupplied, default bounds will be
  // used.
  gfx::Rect initial_bounds;

  // Whether the browser window is displaying only a trusted source, in which
  // case some security UI may not be shown.
  bool is_trusted_source = false;

  // The app name associated with the browser window.
  std::string app_name;

  // The initial state of the browser window.
  ui::mojom::WindowShowState initial_show_state =
      ui::mojom::WindowShowState::kDefault;
};

// Creates a new browser window according to the given `create_params`.
// This may fail, in which case null is returned.
BrowserWindowInterface* CreateBrowserWindow(
    BrowserWindowCreateParams create_params);

#endif  // CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_CREATE_BROWSER_WINDOW_H_
