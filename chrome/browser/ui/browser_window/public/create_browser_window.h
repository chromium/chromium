// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_CREATE_BROWSER_WINDOW_H_
#define CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_CREATE_BROWSER_WINDOW_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/gfx/geometry/rect.h"

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
//
// This may fail, in which case null is returned.
//
// Otherwise, a `BrowserWindowInterface` will be synchronously returned.
// However, due to OS differences, we can't guarantee the browser window is
// fully initialized. If the browser window isn't fully initialized, calls to
// `BrowserWindowInterface` APIs that will change the window will be queued
// first, then executed once the OS has fully initialized the window. We
// recommend all code calling this function to anticipate this scenario.
//
// Detailed behavior for the returned `BrowserWindowInterface`:
//
// GetUnownedUserDataHost() will always return an initialized
// UnownedUserDataHost. However, we can't guarantee there is any feature
// associated with it.
//
// GetWindow() will return a ui::BaseWindow object that's not guaranteed to be
// backed by a real window in the OS. If there is no real window, functions of
// ui::BaseWindow have the following behavior:
// (1) functions updating window states will be queued and executed when the OS
// has initialized the real window;
// (2) functions returning window states will return the predicted states based
// on `create_params` and the queued calls to functions in (1).
//
// For each of the following Get*() functions, it will return the corresponding
// value associated with the window, and the value will remain constant for the
// lifetime of the window:
// (1) GetProfile() will return the Profile associated with the window;
// (2) GetSessionID() will return the session ID associated with the window;
// (3) GetType() will return the type of the window.
//
// OpenURL() isn't guaranteed to work as it may require the OS to initialize
// the window and associate with UnownedUserDataHost features that facilitate
// browser navigation. If OpenURL() can't work, calling it will cause a crash.
//
// If you need to ensure the browser window is fully initialized, please use the
// asynchronous version of this function.
BrowserWindowInterface* CreateBrowserWindow(
    BrowserWindowCreateParams create_params);

// The asynchronous version of `CreateBrowserWindow`. The given `callback` will
// always be invoked asynchronously with the newly created
// `BrowserWindowInterface`.
//
// On all platforms, if the `BrowserWindowInterface` passed to the `callback` is
// not null, the `BrowserWindowInterface` has been fully initialized.
void CreateBrowserWindow(
    BrowserWindowCreateParams create_params,
    base::OnceCallback<void(BrowserWindowInterface*)> callback);

// Returns whether a browser window can currently be created for the specified
// // profile. This condition may change during runtime for a given `profile`
// (e.g. a profile may support Browser windows but creating a Browser is
// disallowed during shutdown).
BrowserWindowInterface::CreationStatus GetBrowserWindowCreationStatusForProfile(
    Profile& profile);

#endif  // CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_CREATE_BROWSER_WINDOW_H_
