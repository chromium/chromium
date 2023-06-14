// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_BROWSER_UI_COOKIE_CONTROLS_VIEW_H_
#define COMPONENTS_CONTENT_SETTINGS_BROWSER_UI_COOKIE_CONTROLS_VIEW_H_

#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "components/content_settings/core/common/cookie_controls_breakage_confidence_level.h"
#include "components/content_settings/core/common/cookie_controls_enforcement.h"
#include "components/content_settings/core/common/cookie_controls_status.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content_settings {

// Interface for the old CookieControls observer.
// TODO(crbug.com/1446230): Remove and clean up after UserBypassUI is launched.
class OldCookieControlsObserver : public base::CheckedObserver {
 public:
  virtual void OnStatusChanged(CookieControlsStatus status,
                               CookieControlsEnforcement enforcement,
                               int allowed_cookies,
                               int blocked_cookies) = 0;
  virtual void OnCookiesCountChanged(int allowed_cookies,
                                     int blocked_cookies) = 0;
  virtual void OnStatefulBounceCountChanged(int bounce_count) = 0;
};

// Interface for the CookieControls observer.
class CookieControlsObserver : public base::CheckedObserver {
 public:
  // Called when the third-party cookie blocking status has changed or when
  // cookie  setting was changed. Also called as part of UI initialization to
  // trigger the update. Replaces previous `OnStatusChanged()` for the new UIs.
  virtual void OnStatusChanged(
      // 3PC blocking status: whether 3PC allowed by default, blocked by default
      // or allowed for the site only.
      CookieControlsStatus status,
      // Represents if cookie settings are enforced (ex. by policy).
      CookieControlsEnforcement enforcement,
      // The expiration time of the active UB exception if it is present.
      absl::optional<base::Time> expiration) = 0;

  // Called whenever `OnStatusChanged()` is called and whenever site data is
  // accessed. The site counts are the number of sites that are allowed to or
  // are blocked from accessing site data. There might be reasons other than
  // 3PCB to why a site is blocked or allowed (ex. site data exceptions).
  virtual void OnSitesCountChanged(int allowed_sites, int blocked_sites) = 0;

  // Called wherever the site breakage confidence level changes. It takes into
  // account blocked third-party cookie access, exceptions lifecycle, site
  // engagement index and recent user activity (like frequent page reloads).
  virtual void OnBreakageConfidenceLevelChanged(
      CookieControlsBreakageConfidenceLevel level) = 0;
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_BROWSER_UI_COOKIE_CONTROLS_VIEW_H_
