// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_OLD_COOKIE_CONTROLS_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_OLD_COOKIE_CONTROLS_ICON_VIEW_H_

#include <memory>
#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/location_bar/old_cookie_controls_bubble_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "components/content_settings/browser/ui/cookie_controls_controller.h"
#include "components/content_settings/browser/ui/cookie_controls_view.h"
#include "components/content_settings/core/common/cookie_controls_breakage_confidence_level.h"
#include "components/content_settings/core/common/cookie_controls_status.h"
#include "ui/base/metadata/metadata_header_macros.h"

// View for the cookie control icon in the Omnibox.  This is the old version of
// cookie controls.
//
// TODO(https://crbug.com/1446230): Remove this after the new version is
// launched.
class OldCookieControlsIconView
    : public PageActionIconView,
      public content_settings::OldCookieControlsObserver {
 public:
  METADATA_HEADER(OldCookieControlsIconView);
  OldCookieControlsIconView(
      IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
      PageActionIconView::Delegate* page_action_icon_delegate);
  OldCookieControlsIconView(const OldCookieControlsIconView&) = delete;
  OldCookieControlsIconView& operator=(const OldCookieControlsIconView&) =
      delete;
  ~OldCookieControlsIconView() override;

  // OldCookieControlsObserver:
  void OnStatusChanged(CookieControlsStatus status,
                       CookieControlsEnforcement enforcement,
                       int allowed_cookies,
                       int blocked_cookies) override;
  void OnCookiesCountChanged(int allowed_cookies, int blocked_cookies) override;
  void OnStatefulBounceCountChanged(int bounce_count) override;

  // PageActionIconView:
  views::BubbleDialogDelegate* GetBubble() const override;
  void UpdateImpl() override;

 protected:
  void OnExecuting(PageActionIconView::ExecuteSource source) override;
  const gfx::VectorIcon& GetVectorIcon() const override;

 private:
  bool GetAssociatedBubble() const;
  bool ShouldBeVisible() const;

  // Set confidence_changed = true to animate if the confidence level changed
  // even if the icon is already visible.
  void UpdateIconView(bool confidence_changed = false);
  absl::optional<int> GetLabelForStatus() const;

  CookieControlsStatus status_ = CookieControlsStatus::kUninitialized;
  bool has_blocked_cookies_ = false;
  bool has_blocked_sites_ = false;

  CookieControlsBreakageConfidenceLevel confidence_ =
      CookieControlsBreakageConfidenceLevel::kUninitialized;

  std::unique_ptr<content_settings::CookieControlsController> controller_;
  base::ScopedObservation<content_settings::CookieControlsController,
                          content_settings::OldCookieControlsObserver>
      old_controller_observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_OLD_COOKIE_CONTROLS_ICON_VIEW_H_
