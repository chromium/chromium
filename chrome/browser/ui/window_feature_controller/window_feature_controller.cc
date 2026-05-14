// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/window_feature_controller/window_feature_controller.h"

#include "base/check.h"
#include "base/check_deref.h"
#include "chrome/browser/ui/fullscreen/browser_window_fullscreen_controller.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"

DEFINE_USER_DATA(WindowFeatureController);

WindowFeatureController::WindowFeatureController(
    BrowserWindowFullscreenController* fullscreen_controller,
    web_app::AppBrowserController* app_controller,
    BrowserWindowInterface::Type browser_type,
    bool is_trusted_source,
    ui::UnownedUserDataHost& host)
    : fullscreen_controller_(CHECK_DEREF(fullscreen_controller)),
      app_controller_(app_controller),
      browser_type_(browser_type),
      is_trusted_source_(is_trusted_source),
      scoped_unowned_user_data_(host, *this) {}

WindowFeatureController::~WindowFeatureController() = default;

// static
WindowFeatureController* WindowFeatureController::From(
    BrowserWindowInterface* browser) {
  return ui::ScopedUnownedUserData<WindowFeatureController>::Get(
      browser->GetUnownedUserDataHost());
}

// static
const WindowFeatureController* WindowFeatureController::From(
    const BrowserWindowInterface* browser) {
  return ui::ScopedUnownedUserData<WindowFeatureController>::Get(
      browser->GetUnownedUserDataHost());
}

bool WindowFeatureController::SupportsWindowFeature(
    WindowFeature feature) const {
  bool supports =
      SupportsWindowFeatureImpl(feature, /*check_can_support=*/false);
  // Supported features imply CanSupportWindowFeature.
  DCHECK(!supports || CanSupportWindowFeature(feature));
  return supports;
}

bool WindowFeatureController::CanSupportWindowFeature(
    WindowFeature feature) const {
  return SupportsWindowFeatureImpl(feature, /*check_can_support=*/true);
}

bool WindowFeatureController::NormalBrowserSupportsWindowFeature(
    WindowFeature feature,
    bool check_can_support) const {
  switch (feature) {
    case WindowFeature::kFeatureBookmarkBar:
      return true;
    case WindowFeature::kFeatureTabStrip:
    case WindowFeature::kFeatureToolbar:
    case WindowFeature::kFeatureLocationBar:
      return check_can_support || !IsFullscreen();
    case WindowFeature::kFeatureTitleBar:
    case WindowFeature::kFeatureNone:
      return false;
  }
}

bool WindowFeatureController::PopupBrowserSupportsWindowFeature(
    WindowFeature feature,
    bool check_can_support) const {
  switch (feature) {
    case WindowFeature::kFeatureTitleBar:
    case WindowFeature::kFeatureLocationBar:
      return check_can_support || (!IsFullscreen() && !is_trusted_source_);
    case WindowFeature::kFeatureTabStrip:
    case WindowFeature::kFeatureToolbar:
    case WindowFeature::kFeatureBookmarkBar:
    case WindowFeature::kFeatureNone:
      return false;
  }
}

bool WindowFeatureController::AppPopupBrowserSupportsWindowFeature(
    WindowFeature feature,
    bool check_can_support) const {
  switch (feature) {
    case WindowFeature::kFeatureTitleBar:
      return check_can_support || !IsFullscreen();
    case WindowFeature::kFeatureLocationBar:
      return app_controller_ && (check_can_support || !IsFullscreen());
    default:
      return PopupBrowserSupportsWindowFeature(feature, check_can_support);
  }
}

bool WindowFeatureController::AppBrowserSupportsWindowFeature(
    WindowFeature feature,
    bool check_can_support) const {
  DCHECK(app_controller_);
  switch (feature) {
    // Web apps should always support the toolbar, so the title/origin of the
    // current page can be shown when browsing a url that is not inside the app.
    // Note: Final determination of whether or not the toolbar is shown is made
    // by the |AppBrowserController|.
    // TODO(crbug.com/40639933): Make this control the visibility of Browser
    // Controls more generally.
    case WindowFeature::kFeatureToolbar:
      return true;
    case WindowFeature::kFeatureTitleBar:
    // TODO(crbug.com/40639933): Make this control the visibility of
    // CustomTabBarView.
    case WindowFeature::kFeatureLocationBar:
      return check_can_support || !IsFullscreen();
    case WindowFeature::kFeatureTabStrip:
      // Even when the app has a tab strip, it should be hidden in
      // fullscreen. This is consistent with the behavior of
      // NormalBrowserSupportsWindowFeature().
      return app_controller_->has_tab_strip() &&
             (check_can_support || !IsFullscreen());
    case WindowFeature::kFeatureBookmarkBar:
    case WindowFeature::kFeatureNone:
      return false;
  }
}

bool WindowFeatureController::PictureInPictureBrowserSupportsWindowFeature(
    WindowFeature feature,
    bool check_can_support) const {
  switch (feature) {
    case WindowFeature::kFeatureTitleBar:
      return true;
    case WindowFeature::kFeatureLocationBar:
    case WindowFeature::kFeatureTabStrip:
    case WindowFeature::kFeatureToolbar:
    case WindowFeature::kFeatureBookmarkBar:
    case WindowFeature::kFeatureNone:
      return false;
  }
}

bool WindowFeatureController::SupportsWindowFeatureImpl(
    WindowFeature feature,
    bool check_can_support) const {
  switch (browser_type_) {
    case BrowserWindowInterface::Type::TYPE_NORMAL:
      return NormalBrowserSupportsWindowFeature(feature, check_can_support);
    case BrowserWindowInterface::Type::TYPE_POPUP:
      return PopupBrowserSupportsWindowFeature(feature, check_can_support);
    case BrowserWindowInterface::Type::TYPE_APP:
      if (app_controller_) {
        return AppBrowserSupportsWindowFeature(feature, check_can_support);
      }
      // TODO(crbug.com/40639933): Change legacy apps to TYPE_APP_POPUP.
      return AppPopupBrowserSupportsWindowFeature(feature, check_can_support);
    case BrowserWindowInterface::Type::TYPE_DEVTOOLS:
    case BrowserWindowInterface::Type::TYPE_APP_POPUP:
      return AppPopupBrowserSupportsWindowFeature(feature, check_can_support);
    case BrowserWindowInterface::Type::TYPE_PICTURE_IN_PICTURE:
      return PictureInPictureBrowserSupportsWindowFeature(feature,
                                                          check_can_support);
  }
}

bool WindowFeatureController::IsFullscreen() const {
  return fullscreen_controller_->ShouldHideUIForFullscreen();
}
