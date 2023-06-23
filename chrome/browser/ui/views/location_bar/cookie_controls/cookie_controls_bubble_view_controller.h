// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_COOKIE_CONTROLS_COOKIE_CONTROLS_BUBBLE_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_COOKIE_CONTROLS_COOKIE_CONTROLS_BUBBLE_VIEW_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_bubble_view.h"
#include "components/content_settings/browser/ui/cookie_controls_controller.h"
#include "components/content_settings/browser/ui/cookie_controls_view.h"
#include "components/content_settings/core/common/cookie_controls_enforcement.h"
#include "components/content_settings/core/common/cookie_controls_status.h"

class CookieControlsBubbleView;

class CookieControlsBubbleViewController
    : public content_settings::CookieControlsObserver {
 public:
  CookieControlsBubbleViewController(
      CookieControlsBubbleView* bubble_view,
      content_settings::CookieControlsController* controller);
  ~CookieControlsBubbleViewController() override;

  explicit CookieControlsBubbleViewController(
      content_settings::CookieControlsController* controller);

  // CookieControlsObserver:
  void OnStatusChanged(CookieControlsStatus status,
                       CookieControlsEnforcement enforcement,
                       base::Time expiration) override;
  void OnSitesCountChanged(int allowed_sites, int blocked_sites) override;
  void OnBreakageConfidenceLevelChanged(
      CookieControlsBreakageConfidenceLevel level) override;

 private:
  raw_ptr<CookieControlsBubbleView> bubble_view_;
  base::WeakPtr<content_settings::CookieControlsController> controller_;

  base::ScopedObservation<content_settings::CookieControlsController,
                          content_settings::CookieControlsObserver>
      controller_observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_COOKIE_CONTROLS_COOKIE_CONTROLS_BUBBLE_VIEW_CONTROLLER_H_
