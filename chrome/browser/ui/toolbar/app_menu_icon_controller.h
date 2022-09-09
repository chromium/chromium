// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_APP_MENU_ICON_CONTROLLER_H_
#define CHROME_BROWSER_UI_TOOLBAR_APP_MENU_ICON_CONTROLLER_H_

#include <stdint.h>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "chrome/browser/ui/global_error/global_error_observer.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/upgrade_detector/upgrade_observer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/models/image_model.h"

class Profile;
class UpgradeDetector;

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

    // Get the appropriate colors for various severity levels.
    virtual SkColor GetDefaultColorForSeverity(Severity severity) const = 0;

   protected:
    virtual ~Delegate() {}
  };

  // Creates an instance of this class for the given |profile| that will notify
  // |delegate| of updates.
  AppMenuIconController(Profile* profile, Delegate* delegate);
  AppMenuIconController(UpgradeDetector* upgrade_detector,
                        Profile* profile,
                        Delegate* delegate);

  AppMenuIconController(const AppMenuIconController&) = delete;
  AppMenuIconController& operator=(const AppMenuIconController&) = delete;

  ~AppMenuIconController() override;

  // Forces an update of the UI based on the current state of the world. This
  // will check whether there are any current pending updates, global errors,
  // etc. and based on that information trigger an appropriate call to the
  // delegate.
  void UpdateDelegate();

  // Returns the icon type and severity based on the current state.
  TypeAndSeverity GetTypeAndSeverity() const;

  // Gets the color to be used for the app menu's icon.
  // |severity_none_color|, if provided, will be used when the Severity is NONE.
  // Otherwise the basic toolbar button icon color will be used.
  SkColor GetIconColor(
      const absl::optional<SkColor>& severity_none_color) const;

 private:
  // GlobalErrorObserver:
  void OnGlobalErrorsChanged() override;

  // UpgradeObserver:
  void OnUpgradeRecommended() override;

  // True for desktop Chrome on dev and canary channels.
  const bool is_unstable_channel_;
  const raw_ptr<UpgradeDetector> upgrade_detector_;
  const raw_ptr<Profile> profile_;
  const raw_ptr<Delegate> delegate_;
  base::ScopedObservation<GlobalErrorService, GlobalErrorObserver>
      global_error_observation_{this};
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_APP_MENU_ICON_CONTROLLER_H_
