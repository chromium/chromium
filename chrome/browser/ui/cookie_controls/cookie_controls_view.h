// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COOKIE_CONTROLS_COOKIE_CONTROLS_VIEW_H_
#define CHROME_BROWSER_UI_COOKIE_CONTROLS_COOKIE_CONTROLS_VIEW_H_

#include "base/observer_list_types.h"
#include "chrome/browser/ui/cookie_controls/cookie_controls_controller.h"

// Interface for the CookieControls UI.
class CookieControlsView : public base::CheckedObserver {
 public:
  virtual void OnStatusChanged(CookieControlsController::Status status,
                               int blocked_cookies) = 0;
  virtual void OnBlockedCookiesCountChanged(int blocked_cookies) = 0;
};

#endif  // CHROME_BROWSER_UI_COOKIE_CONTROLS_COOKIE_CONTROLS_VIEW_H_
