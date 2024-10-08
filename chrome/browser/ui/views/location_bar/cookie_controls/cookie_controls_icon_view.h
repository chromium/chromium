// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_COOKIE_CONTROLS_COOKIE_CONTROLS_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_COOKIE_CONTROLS_COOKIE_CONTROLS_ICON_VIEW_H_

#include <memory>

#include "base/scoped_observation.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_bubble_coordinator.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "components/content_settings/browser/ui/cookie_controls_controller.h"
#include "components/content_settings/browser/ui/cookie_controls_view.h"
#include "components/content_settings/core/common/cookie_blocking_3pcd_status.h"
#include "components/user_education/common/feature_promo_result.h"
#include "ui/base/metadata/metadata_header_macros.h"

// View for the cookie control icon in the Omnibox.  This is the new version of
// the cookie controls (https://crbug.com/1446230).
class CookieControlsIconView : public PageActionIconView,
                               public content_settings::CookieControlsObserver {
  METADATA_HEADER(CookieControlsIconView, PageActionIconView)

 public:
  CookieControlsIconView(
      Browser* browser,
      IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
      PageActionIconView::Delegate* page_action_icon_delegate);
  CookieControlsIconView(const CookieControlsIconView&) = delete;
  CookieControlsIconView& operator=(const CookieControlsIconView&) = delete;
  ~CookieControlsIconView() override;

  // CookieControlsObserver:
  void OnCookieControlsIconStatusChanged(
      bool icon_visible,
      bool protections_on,
      CookieBlocking3pcdStatus blocking_status,
      bool should_highlight) override;
  void OnFinishedPageReloadWithChangedSettings() override;

  void ShowCookieControlsBubble();

  // PageActionIconView:
  views::BubbleDialogDelegate* GetBubble() const override;
  void UpdateImpl() override;

  CookieControlsBubbleCoordinator* GetCoordinatorForTesting() const;
  void SetCoordinatorForTesting(
      std::unique_ptr<CookieControlsBubbleCoordinator> coordinator);

  void DisableUpdatesForTesting();

 protected:
  void OnExecuting(PageActionIconView::ExecuteSource source) override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  void UpdateTooltipForFocus() override;

 private:
  friend class CookieControlsIconViewUnitTest;

  bool GetAssociatedBubble() const;
  bool ShouldBeVisible() const;
  // Whether a managed IPH is currently active.
  bool IsManagedIPHActive() const;
  void OnIPHClosed();

  // Attempts to show IPH for the cookie controls icon.
  void MaybeShowIPH();
  // Callback for when we try to show the IPH.
  void OnShowPromoResult(user_education::FeaturePromoResult result);

  void MaybeAnimateIcon();
  void UpdateIcon();

  int GetLabelForStatus() const;
  void SetLabelForStatus();

  // Whether to use "Tracking Protection" for label and tooltip.
  bool ShouldShowTrackingProtectionText();

  bool icon_visible_ = false;
  bool protections_on_ = false;
  bool protections_changed_ = true;
  bool did_animate_ = false;
  // Whether we should have a visual indicator highlighting the icon.
  bool should_highlight_ = false;
  GURL last_visited_url_;

  // True if calls to UpdateImpl should noop for testing purposes.
  // TODO: 344042974 - Remove this once the issue has been resolved.
  bool disable_updates_for_testing_ = false;

  CookieBlocking3pcdStatus blocking_status_ =
      CookieBlocking3pcdStatus::kNotIn3pcd;

  raw_ptr<Browser> browser_ = nullptr;

  std::unique_ptr<content_settings::CookieControlsController> controller_;
  std::unique_ptr<CookieControlsBubbleCoordinator> bubble_coordinator_ =
      nullptr;
  base::ScopedObservation<content_settings::CookieControlsController,
                          content_settings::CookieControlsObserver>
      controller_observation_{this};
  base::WeakPtrFactory<CookieControlsIconView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_COOKIE_CONTROLS_COOKIE_CONTROLS_ICON_VIEW_H_
