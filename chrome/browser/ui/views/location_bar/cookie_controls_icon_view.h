// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_COOKIE_CONTROLS_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_COOKIE_CONTROLS_ICON_VIEW_H_

#include <memory>
#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls_bubble_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "components/content_settings/browser/ui/cookie_controls_controller.h"
#include "components/content_settings/browser/ui/cookie_controls_view.h"
#include "components/content_settings/core/common/cookie_controls_status.h"
#include "ui/base/metadata/metadata_header_macros.h"

// View for the cookie control icon in the Omnibox.
class CookieControlsIconView : public PageActionIconView,
                               public content_settings::CookieControlsView {
 public:
  METADATA_HEADER(CookieControlsIconView);
  CookieControlsIconView(
      IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
      PageActionIconView::Delegate* page_action_icon_delegate);
  CookieControlsIconView(const CookieControlsIconView&) = delete;
  CookieControlsIconView& operator=(const CookieControlsIconView&) = delete;
  ~CookieControlsIconView() override;

  // CookieControlsUI:
  void OnStatusChanged(CookieControlsStatus status,
                       CookieControlsEnforcement enforcement,
                       int allowed_cookies,
                       int blocked_cookies) override;
  void OnCookiesCountChanged(int allowed_cookies, int blocked_cookies) override;

  // PageActionIconView:
  views::BubbleDialogDelegate* GetBubble() const override;
  void UpdateImpl() override;
  std::u16string GetTextForTooltipAndAccessibleName() const override;

 protected:
  void OnExecuting(PageActionIconView::ExecuteSource source) override;
  const gfx::VectorIcon& GetVectorIcon() const override;

 private:
  bool GetAssociatedBubble() const;
  bool ShouldBeVisible() const;

  CookieControlsStatus status_ = CookieControlsStatus::kUninitialized;
  bool has_blocked_cookies_ = false;

  std::unique_ptr<content_settings::CookieControlsController> controller_;
  base::ScopedObservation<content_settings::CookieControlsController,
                          content_settings::CookieControlsView>
      observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_COOKIE_CONTROLS_ICON_VIEW_H_
