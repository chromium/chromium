// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_BROWSER_UI_COOKIE_CONTROLS_VIEW_H_
#define COMPONENTS_CONTENT_SETTINGS_BROWSER_UI_COOKIE_CONTROLS_VIEW_H_

#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "components/content_settings/core/common/cookie_blocking_3pcd_status.h"
#include "components/content_settings/core/common/cookie_controls_enforcement.h"
#include "components/content_settings/core/common/tracking_protection_feature.h"

namespace content_settings {

// Interface for the CookieControls observer.
class CookieControlsObserver : public base::CheckedObserver {
 public:
  // Called when the third-party cookie blocking status has changed or when
  // cookie  setting was changed. Also called as part of UI initialization to
  // trigger the update. Replaces previous `OnStatusChanged()` for the new UIs.
  virtual void OnStatusChanged(
      // Whether Tracking Protection controls should be shown.
      bool controls_visible,
      // Whether protections (3PC blocking and ACT features) are on for the
      // current site. NOTE: for the 3PC toggle this is true when the toggle is
      // off/3PC are blocked.
      bool protections_on,
      // Represents if cookie settings are enforced (ex. by policy).
      CookieControlsEnforcement enforcement,
      // 3PC blocking status for 3PCD: whether 3PC are limited or all blocked.
      CookieBlocking3pcdStatus blocking_status,
      // The expiration time of the active UB exception if it is present.
      base::Time expiration,
      // Contains all information needed by the UI to render ACT feature states
      // within in-context surfaces (user bypass, page info).
      // TODO(https://b/340312239): Remove `enforcement` param once this list
      // is being used.
      std::vector<TrackingProtectionFeature> features) {}

  // Called to update the user bypass entrypoint in the omnibox. This can impact
  // any property of the entrypoint (i.e. the visibility, label, or icon).
  virtual void OnCookieControlsIconStatusChanged(
      // Whether to show the user bypass icon.
      bool icon_visible,
      // Whether protections (3PC blocking and ACT features) are on for the
      // current site. NOTE: for the 3PC toggle this is true when the toggle is
      // off/3PC are blocked.
      bool protections_on,
      // 3PC blocking status for 3PCD: whether 3PC are limited or all blocked.
      CookieBlocking3pcdStatus blocking_status,
      // Whether we should highlight the user bypass icon.
      bool should_highlight) {}

  // Called when the current page has finished reloading, after the effective
  // cookie setting was changed on the previous load via the controller.
  virtual void OnFinishedPageReloadWithChangedSettings() {}

  // Called when the number of recent page reloads exceeds the highlight
  // heuristic. Intended for use in Clank PWA logic.
  virtual void OnReloadThresholdExceeded() {}
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_BROWSER_UI_COOKIE_CONTROLS_VIEW_H_
