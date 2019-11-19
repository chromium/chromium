// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_APP_MENU_ICON_CONTROLLER_H_
#define CHROME_BROWSER_UI_TOOLBAR_APP_MENU_ICON_CONTROLLER_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "build/build_config.h"
#include "chrome/browser/ui/global_error/global_error_observer.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/upgrade_detector/upgrade_observer.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image_skia.h"

class Profile;
class UpgradeDetector;

namespace ui {
class NativeTheme;
class ThemeProvider;
}  // namespace ui

// AppMenuIconController encapsulates the logic for badging the app menu icon
// as a result of various events - such as available updates, errors, etc.
class AppMenuIconController : public GlobalErrorObserver,
                              public UpgradeObserver {
 public:
  enum class IconType {
    NONE,
    UPGRADE_NOTIFICATION,
    GLOBAL_ERROR,
  };
  enum class Severity {
    NONE,
    LOW,
    MEDIUM,
    HIGH,
  };

  // The app menu icon's type and severity.
  struct TypeAndSeverity {
    IconType type;
    Severity severity;
  };

  // Delegate interface for receiving icon update notifications.
  class Delegate {
   public:
    // Notifies the UI to update the icon to have the specified
    // |type_and_severity|.
    virtual void UpdateTypeAndSeverity(TypeAndSeverity type_and_severity) = 0;

    // Accessors for properties of the View hosting the controller.
    virtual const ui::ThemeProvider* GetViewThemeProvider() const = 0;
    virtual ui::NativeTheme* GetViewNativeTheme() = 0;

   protected:
    virtual ~Delegate() {}
  };

  // Creates an instance of this class for the given |profile| that will notify
  // |delegate| of updates.
  AppMenuIconController(Profile* profile, Delegate* delegate);
  AppMenuIconController(UpgradeDetector* upgrade_detector,
                        Profile* profile,
                        Delegate* delegate);
  ~AppMenuIconController() override;

  // Forces an update of the UI based on the current state of the world. This
  // will check whether there are any current pending updates, global errors,
  // etc. and based on that information trigger an appropriate call to the
  // delegate.
  void UpdateDelegate();

  // Returns the icon type and severity based on the current state.
  TypeAndSeverity GetTypeAndSeverity() const;

  // Returns the image to be used for the app menu's icon and the upgrade item
  // in the app menu (when the IconType is UPGRADE_NOTIFICATION). |touch_ui|
  // indicates whether the touch-friendly variant is requested.
  // |promo_highlight_color|, if provided, overrides the basic color when the
  // app menu icon's Severity is NONE.
  gfx::ImageSkia GetIconImage(
      bool touch_ui,
      base::Optional<SkColor> promo_highlight_color = base::nullopt) const;

  // Gets the color to be used for the app menu's icon. |promo_highlight_color|,
  // if provided, overrides the basic color when the icon's Severity is NONE.
  SkColor GetIconColor(base::Optional<SkColor> promo_highlight_color) const;

 private:
  // GlobalErrorObserver:
  void OnGlobalErrorsChanged() override;

  // UpgradeObserver:
  void OnUpgradeRecommended() override;

  // True for desktop Chrome on dev and canary channels.
  const bool is_unstable_channel_;
  UpgradeDetector* const upgrade_detector_;
  Profile* const profile_;
  Delegate* const delegate_;
  ScopedObserver<GlobalErrorService, GlobalErrorObserver>
      global_error_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(AppMenuIconController);
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_APP_MENU_ICON_CONTROLLER_H_
