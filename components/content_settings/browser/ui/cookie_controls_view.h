// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_BROWSER_UI_COOKIE_CONTROLS_VIEW_H_
#define COMPONENTS_CONTENT_SETTINGS_BROWSER_UI_COOKIE_CONTROLS_VIEW_H_

#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "components/content_settings/core/common/cookie_controls_enforcement.h"
#include "components/content_settings/core/common/cookie_controls_state.h"

namespace content_settings {

// Interface for the CookieControls observer.
class CookieControlsObserver : public base::CheckedObserver {
 public:
  // Called when the state we display in the cookie controls UI has changed.
  // Also called as part of UI initialization to trigger the update
  virtual void OnStatusChanged(
      // The state of the controls for the UI to change.
      CookieControlsState controls_state,
      // Represents if cookie settings are enforced (ex. by policy).
      CookieControlsEnforcement enforcement,
      // The expiration time of the active UB exception if it is present.
      base::Time expiration) {}

  // Called to update the cookie controls icon in the omnibox. This can impact
  // any property of the icon, including the visibility and label.
  virtual void OnCookieControlsIconStatusChanged(
      // The state of the controls for the UI to change.
      CookieControlsState controls_state) {}

  // Called when the bubble should be closed (e.g. due to a successful page
  // reload or another UI element being shown).
  virtual void OnBubbleCloseTriggered() {}
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_BROWSER_UI_COOKIE_CONTROLS_VIEW_H_
