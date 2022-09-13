// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_BROWSER_UI_COOKIE_CONTROLS_VIEW_H_
#define COMPONENTS_CONTENT_SETTINGS_BROWSER_UI_COOKIE_CONTROLS_VIEW_H_

#include "base/observer_list_types.h"
#include "components/content_settings/core/common/cookie_controls_enforcement.h"
#include "components/content_settings/core/common/cookie_controls_status.h"

namespace content_settings {

// Interface for the CookieControls UI.
class CookieControlsView : public base::CheckedObserver {
 public:
  virtual void OnStatusChanged(CookieControlsStatus status,
                               CookieControlsEnforcement enforcement,
                               int allowed_cookies,
                               int blocked_cookies) = 0;
  virtual void OnCookiesCountChanged(int allowed_cookies,
                                     int blocked_cookies) = 0;
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_BROWSER_UI_COOKIE_CONTROLS_VIEW_H_
