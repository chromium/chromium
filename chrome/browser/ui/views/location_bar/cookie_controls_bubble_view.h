// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_COOKIE_CONTROLS_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_COOKIE_CONTROLS_BUBBLE_VIEW_H_

#include "base/macros.h"
#include "base/optional.h"
#include "chrome/browser/ui/cookie_controls/cookie_controls_service.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "components/content_settings/browser/ui/cookie_controls_controller.h"
#include "components/content_settings/browser/ui/cookie_controls_view.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/cookie_controls_enforcement.h"
#include "components/content_settings/core/common/cookie_controls_status.h"
#include "ui/views/bubble/tooltip_icon.h"
#include "ui/views/controls/button/button.h"

namespace content {
class WebContents;
}

namespace views {
class ImageView;
class Label;
}  // namespace views

// View used to display the cookie controls ui.
class CookieControlsBubbleView : public LocationBarBubbleDelegateView,
                                 public views::TooltipIcon::Observer,
                                 public content_settings::CookieControlsView {
 public:
  static void ShowBubble(views::View* anchor_view,
                         views::Button* highlighted_button,
                         content::WebContents* web_contents,
                         content_settings::CookieControlsController* controller,
                         CookieControlsStatus status);

  static CookieControlsBubbleView* GetCookieBubble();

  // content_settings::CookieControlsView:
  void OnStatusChanged(CookieControlsStatus status,
                       CookieControlsEnforcement enforcement,
                       int allowed_cookies,
                       int blocked_cookies) override;
  void OnCookiesCountChanged(int allowed_cookies, int blocked_cookies) override;

 private:
  enum class IntermediateStep {
    kNone,
    // Show a button to disable cookie blocking on the current site.
    kTurnOffButton,
  };

  CookieControlsBubbleView(
      views::View* anchor_view,
      content::WebContents* web_contents,
      content_settings::CookieControlsController* cookie_contols);
  ~CookieControlsBubbleView() override;

  void UpdateUi();

  // LocationBarBubbleDelegateView:
  void CloseBubble() override;
  void Init() override;
  std::u16string GetWindowTitle() const override;
  void WindowClosing() override;
  gfx::Size CalculatePreferredSize() const override;
  void AddedToWidget() override;

  void ShowCookiesLinkClicked();
  void NotWorkingLinkClicked();
  void OnDialogAccepted();

  // views::TooltipIcon::Observer:
  void OnTooltipBubbleShown(views::TooltipIcon* icon) override;
  void OnTooltipIconDestroying(views::TooltipIcon* icon) override;

  content_settings::CookieControlsController* controller_ = nullptr;

  CookieControlsStatus status_ = CookieControlsStatus::kUninitialized;

  CookieControlsEnforcement enforcement_ =
      CookieControlsEnforcement::kNoEnforcement;

  IntermediateStep intermediate_step_ = IntermediateStep::kNone;

  base::Optional<int> blocked_cookies_;

  views::ImageView* header_view_ = nullptr;
  views::Label* text_ = nullptr;
  views::View* extra_view_ = nullptr;
  views::View* show_cookies_link_ = nullptr;

  base::ScopedObservation<content_settings::CookieControlsController,
                          content_settings::CookieControlsView>
      controller_observation_{this};
  base::ScopedObservation<views::TooltipIcon, views::TooltipIcon::Observer>
      tooltip_observation_{this};

  DISALLOW_COPY_AND_ASSIGN(CookieControlsBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_COOKIE_CONTROLS_BUBBLE_VIEW_H_
