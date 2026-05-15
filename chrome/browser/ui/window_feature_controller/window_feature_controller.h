// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WINDOW_FEATURE_CONTROLLER_WINDOW_FEATURE_CONTROLLER_H_
#define CHROME_BROWSER_UI_WINDOW_FEATURE_CONTROLLER_WINDOW_FEATURE_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowFullscreenController;

namespace web_app {
class AppBrowserController;
}

// Controls and queries whether specific UI elements (toolbar, tabstrip,
// bookmark bar) are supported or currently visible on a browser window.
class WindowFeatureController {
 public:
  DECLARE_USER_DATA(WindowFeatureController);

  // Possible elements of the Browser window.
  enum class WindowFeature {
    kFeatureNone,
    kFeatureTitleBar,
    kFeatureTabStrip,
    kFeatureToolbar,
    kFeatureLocationBar,
    kFeatureBookmarkBar,
  };

  WindowFeatureController(
      BrowserWindowFullscreenController* fullscreen_controller,
      web_app::AppBrowserController* app_controller,
      BrowserWindowInterface::Type browser_type,
      bool is_trusted_source,
      ui::UnownedUserDataHost& host);
  ~WindowFeatureController();

  WindowFeatureController(const WindowFeatureController&) = delete;
  WindowFeatureController& operator=(const WindowFeatureController&) = delete;

  static WindowFeatureController* From(BrowserWindowInterface* browser);
  static const WindowFeatureController* From(
      const BrowserWindowInterface* browser);

  // Returns true if the Browser supports the specified feature. The value of
  // this varies during the lifetime of the browser. For example, if the window
  // is fullscreen this may return a different value. If you only care about
  // whether or not it's possible for the browser to support a particular
  // feature use |CanSupportWindowFeature|.
  bool SupportsWindowFeature(WindowFeature feature) const;

  // Returns true if the Browser can support the specified feature. See comment
  // in |SupportsWindowFeature| for details on this.
  bool CanSupportWindowFeature(WindowFeature feature) const;

#if BUILDFLAG(IS_MAC)
  // Returns if the window will use immersive fullscreen mode.
  bool UsesImmersiveFullscreenMode() const;

  // Returns if the window will use immersive fullscreen tabbed mode.
  bool UsesImmersiveFullscreenTabbedMode() const;
#endif

 private:
  bool NormalBrowserSupportsWindowFeature(WindowFeature feature,
                                          bool check_can_support) const;
  bool PopupBrowserSupportsWindowFeature(WindowFeature feature,
                                         bool check_can_support) const;
  bool AppPopupBrowserSupportsWindowFeature(WindowFeature feature,
                                            bool check_can_support) const;
  bool AppBrowserSupportsWindowFeature(WindowFeature feature,
                                       bool check_can_support) const;
  bool PictureInPictureBrowserSupportsWindowFeature(
      WindowFeature feature,
      bool check_can_support) const;
  bool SupportsWindowFeatureImpl(WindowFeature feature,
                                 bool check_can_support) const;
  bool IsFullscreen() const;

  const raw_ref<BrowserWindowFullscreenController> fullscreen_controller_;

  // Can be null.
  const raw_ptr<web_app::AppBrowserController> app_controller_;

  const BrowserWindowInterface::Type browser_type_;
  const bool is_trusted_source_;

  ui::ScopedUnownedUserData<WindowFeatureController> scoped_unowned_user_data_;
};

#endif  // CHROME_BROWSER_UI_WINDOW_FEATURE_CONTROLLER_WINDOW_FEATURE_CONTROLLER_H_
