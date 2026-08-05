// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_CREATE_BROWSER_WINDOW_H_
#define CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_CREATE_BROWSER_WINDOW_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/gfx/geometry/rect.h"

#if !BUILDFLAG(IS_ANDROID)
#include "components/tab_groups/tab_group_id.h"
#include "third_party/blink/public/mojom/picture_in_picture_window_options/picture_in_picture_window_options.mojom.h"
#endif

namespace content {

class WebContents;

}  // namespace content

#if !BUILDFLAG(IS_ANDROID)
class BrowserWindow;
#endif

// Parameters used when creating a new browser window.
struct BrowserWindowCreateParams {
  BrowserWindowCreateParams(BrowserWindowInterface::Type type,
                            Profile& profile,
                            bool from_user_gesture);
  BrowserWindowCreateParams(Profile& profile, bool from_user_gesture);

  // Convenience pointer constructors for gradual migration.
  BrowserWindowCreateParams(BrowserWindowInterface::Type type,
                            Profile* profile,
                            bool from_user_gesture);
  BrowserWindowCreateParams(Profile* profile, bool from_user_gesture);

  BrowserWindowCreateParams(BrowserWindowCreateParams&&);
  BrowserWindowCreateParams(const BrowserWindowCreateParams&) = delete;
  BrowserWindowCreateParams& operator=(const BrowserWindowCreateParams&) =
      delete;
  BrowserWindowCreateParams& operator=(BrowserWindowCreateParams&&);
  ~BrowserWindowCreateParams();

#if !BUILDFLAG(IS_ANDROID)
  // Provides explicit cloning for desktop test suites without violating
  // move-only semantics on Android (satisfies crbug.com/413168662).
  BrowserWindowCreateParams Clone() const;

  static BrowserWindowCreateParams CreateForApp(
      const std::string& app_name,
      bool trusted_source,
      const gfx::Rect& window_bounds,
      Profile* profile,
      bool user_gesture);

  static BrowserWindowCreateParams CreateForAppPopup(
      const std::string& app_name,
      bool trusted_source,
      const gfx::Rect& window_bounds,
      Profile* profile,
      bool user_gesture);

  static BrowserWindowCreateParams CreateForPictureInPicture(
      const std::string& app_name,
      bool trusted_source,
      Profile* profile,
      bool user_gesture);

  static BrowserWindowCreateParams CreateForDevTools(Profile* profile);
#endif  // !BUILDFLAG(IS_ANDROID)

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

#if !BUILDFLAG(IS_ANDROID)
  // Represents whether a value was known to be explicitly specified.
  enum class ValueSpecified { kUnknown, kSpecified, kUnspecified };

  // Represents the source of a browser creation request.
  enum class CreationSource {
    kUnknown,
    kSessionRestore,
    kStartupCreator,
    kLastAndUrlsStartupPref,
    kDeskTemplate,
  };

  // Whether this Browser should be omitted from being saved/restored by session
  // restore.
  bool omit_from_session_restore = false;

  // If true, a new window opening should be treated like the start of a session
  // (with potential session restore, startup URLs, etc.). Otherwise, don't
  // restore the session.
  bool should_trigger_session_restore = true;

  // Whether `initial_bounds.origin()` was explicitly specified, if known.
  ValueSpecified initial_origin_specified = ValueSpecified::kUnknown;

  // The workspace the window should open in, if the platform supports it.
  std::string initial_workspace;

  // Whether the window is visible on all workspaces initially, if the
  // platform supports it.
  bool initial_visible_on_all_workspaces_state = false;

  CreationSource creation_source = CreationSource::kUnknown;

  // Whether this browser was created specifically for dragged tab(s).
  bool in_tab_dragging = false;

  // Supply a custom BrowserWindow implementation, to be used instead of the
  // default. Intended for testing.
  raw_ptr<BrowserWindow, DanglingUntriaged> window = nullptr;

  // User-set title of this browser window, if there is one.
  std::string user_title;

  // Only applied when not in forced app mode. True if the browser is
  // resizeable.
  bool can_resize = true;

  // Only applied when not in forced app mode. True if the browser can be
  // maximizable.
  bool can_maximize = true;

  // Only applied when not in forced app mode. True if the browser can enter
  // fullscreen.
  bool can_fullscreen = true;

  // Document Picture in Picture options, specific to TYPE_PICTURE_IN_PICTURE.
  std::optional<blink::mojom::PictureInPictureWindowOptions> pip_options;

  // Specifies the collapsed state for the Vertical Tab Strip. True if the
  // browser is collapsed.
  std::optional<bool> vertical_tab_strip_collapsed;
  // Specifies the width for the uncollapsed Vertical Tab Strip.
  std::optional<int> vertical_tab_strip_uncollapsed_width;

  // Specifies the focused tab group ID, if the window should be created in a
  // focused state.
  std::optional<tab_groups::TabGroupId> focused_tab_group_id;

#if BUILDFLAG(IS_CHROMEOS)
  // If set, the browser should be created on the display given by
  // `display_id`.
  std::optional<int64_t> display_id;
#endif

#if BUILDFLAG(IS_LINUX)
  // When the browser window is shown, the desktop environment is notified
  // using this ID. In response, the desktop will stop playing the "waiting
  // for startup" animation (if any).
  std::string startup_id;
#endif

#if BUILDFLAG(IS_OZONE)
  // Some platforms support session management assisted by the windowing
  // system.
  int32_t restore_id = 0;
#endif
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
  // An optional WebContents to be used when creating the browser window.
  // Note: On Android, calls to CreateBrowserWindow will release this
  // WebContent's ownership to an AndroidBrowserWindowCreateParams object.
  std::unique_ptr<content::WebContents> web_contents;
#endif
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
