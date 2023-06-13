// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_OLD_COOKIE_CONTROLS_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_OLD_COOKIE_CONTROLS_BUBBLE_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/cookie_controls/cookie_controls_service.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "components/content_settings/browser/ui/cookie_controls_controller.h"
#include "components/content_settings/browser/ui/cookie_controls_view.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/cookie_controls_enforcement.h"
#include "components/content_settings/core/common/cookie_controls_status.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/views/bubble/tooltip_icon.h"
#include "ui/views/controls/button/button.h"

namespace content {
class WebContents;
}

namespace views {
class ImageView;
class Label;
}  // namespace views

// Old view used to display the cookie controls ui.
// TODO(crbug.com/1446230): Clean up this old view once the feature is launched.
class OldCookieControlsBubbleView
    : public LocationBarBubbleDelegateView,
      public views::TooltipIcon::Observer,
      public content_settings::OldCookieControlsObserver {
 public:
  enum DialogViewID {
    VIEW_ID_NONE = 0,
    VIEW_ID_COOKIE_CONTROLS_NOT_WORKING_LINK,
  };

  OldCookieControlsBubbleView(const OldCookieControlsBubbleView&) = delete;
  OldCookieControlsBubbleView& operator=(const OldCookieControlsBubbleView&) =
      delete;

  static void ShowBubble(views::View* anchor_view,
                         views::Button* highlighted_button,
                         content::WebContents* web_contents,
                         content_settings::CookieControlsController* controller,
                         CookieControlsStatus status);

  static OldCookieControlsBubbleView* GetCookieBubble();

  // content_settings::OldCookieControlsObserver:
  void OnStatusChanged(CookieControlsStatus status,
                       CookieControlsEnforcement enforcement,
                       int allowed_cookies,
                       int blocked_cookies) override;
  void OnCookiesCountChanged(int allowed_cookies, int blocked_cookies) override;
  void OnStatefulBounceCountChanged(int bounce_count) override;

 private:
  enum class IntermediateStep {
    kNone,
    // Show a button to disable cookie blocking on the current site.
    kTurnOffButton,
  };

  OldCookieControlsBubbleView(
      views::View* anchor_view,
      content::WebContents* web_contents,
      content_settings::CookieControlsController* cookie_contols);
  ~OldCookieControlsBubbleView() override;

  void UpdateUi();

  // LocationBarBubbleDelegateView:
  void CloseBubble() override;
  void Init() override;
  std::u16string GetWindowTitle() const override;
  void WindowClosing() override;
  gfx::Size CalculatePreferredSize() const override;
  void AddedToWidget() override;

  void OnShowCookiesLinkClicked();
  void OnNotWorkingLinkClicked();
  void OnDialogAccepted();

  // views::TooltipIcon::Observer:
  void OnTooltipBubbleShown(views::TooltipIcon* icon) override;
  void OnTooltipIconDestroying(views::TooltipIcon* icon) override;

  base::WeakPtr<content_settings::CookieControlsController> controller_;

  CookieControlsStatus status_ = CookieControlsStatus::kUninitialized;

  CookieControlsEnforcement enforcement_ =
      CookieControlsEnforcement::kNoEnforcement;

  IntermediateStep intermediate_step_ = IntermediateStep::kNone;

  absl::optional<int> blocked_cookies_;
  absl::optional<int> stateful_bounces_;

  raw_ptr<views::ImageView> header_view_ = nullptr;
  raw_ptr<views::Label> text_ = nullptr;
  raw_ptr<views::View> extra_view_ = nullptr;
  raw_ptr<views::View> show_cookies_link_ = nullptr;

  base::ScopedObservation<content_settings::CookieControlsController,
                          content_settings::OldCookieControlsObserver>
      controller_observation_{this};
  base::ScopedObservation<views::TooltipIcon, views::TooltipIcon::Observer>
      tooltip_observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_OLD_COOKIE_CONTROLS_BUBBLE_VIEW_H_
