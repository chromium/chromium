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
#include "components/content_settings/core/common/cookie_controls_breakage_confidence_level.h"
#include "components/content_settings/core/common/cookie_controls_status.h"
#include "ui/base/metadata/metadata_header_macros.h"

// View for the cookie control icon in the Omnibox.  This is the new version of
// the cookie controls (https://crbug.com/1446230).
class CookieControlsIconView : public PageActionIconView,
                               public content_settings::CookieControlsObserver {
 public:
  METADATA_HEADER(CookieControlsIconView);
  CookieControlsIconView(
      Browser* browser,
      IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
      PageActionIconView::Delegate* page_action_icon_delegate);
  CookieControlsIconView(const CookieControlsIconView&) = delete;
  CookieControlsIconView& operator=(const CookieControlsIconView&) = delete;
  ~CookieControlsIconView() override;

  // CookieControlsObserver:
  void OnStatusChanged(CookieControlsStatus status,
                       CookieControlsEnforcement enforcement,
                       base::Time expiration) override;
  void OnSitesCountChanged(int allowed_third_party_sites_count,
                           int blocked_third_party_sites_count) override;
  void OnBreakageConfidenceLevelChanged(
      CookieControlsBreakageConfidenceLevel level) override;
  void OnFinishedPageReloadWithChangedSettings() override;

  void ShowCookieControlsBubble();

  // PageActionIconView:
  views::BubbleDialogDelegate* GetBubble() const override;
  void UpdateImpl() override;

  CookieControlsBubbleCoordinator* GetCoordinatorForTesting() const;
  void SetCoordinatorForTesting(
      std::unique_ptr<CookieControlsBubbleCoordinator> coordinator);

 protected:
  void OnExecuting(PageActionIconView::ExecuteSource source) override;
  const gfx::VectorIcon& GetVectorIcon() const override;

 private:
  friend class CookieControlsIconViewUnitTest;

  bool GetAssociatedBubble() const;
  bool ShouldBeVisible() const;
  void OnIPHClosed();

  // Set confidence_changed = true to animate if the confidence level changed
  // even if the icon is already visible.
  void UpdateVisibilityAndAnimate(bool confidence_changed = false);
  absl::optional<int> GetLabelForStatus() const;

  CookieControlsStatus status_ = CookieControlsStatus::kUninitialized;

  CookieControlsBreakageConfidenceLevel confidence_ =
      CookieControlsBreakageConfidenceLevel::kUninitialized;

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
