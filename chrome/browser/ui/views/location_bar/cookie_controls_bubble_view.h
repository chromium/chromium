// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_COOKIE_CONTROLS_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_COOKIE_CONTROLS_BUBBLE_VIEW_H_

#include "base/macros.h"
#include "base/optional.h"
#include "chrome/browser/ui/cookie_controls/cookie_controls_controller.h"
#include "chrome/browser/ui/cookie_controls/cookie_controls_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/link_listener.h"

namespace content {
class WebContents;
}

namespace views {
class ImageView;
class Label;
class Link;
}  // namespace views

// View used to display the cookie controls ui.
class CookieControlsBubbleView : public LocationBarBubbleDelegateView,
                                 public views::LinkListener,
                                 public CookieControlsView {
 public:
  static void ShowBubble(views::View* anchor_view,
                         views::Button* highlighted_button,
                         content::WebContents* web_contents,
                         CookieControlsController* controller,
                         CookieControlsController::Status status);

  static CookieControlsBubbleView* GetCookieBubble();

  // CookieControlsView:
  void OnStatusChanged(CookieControlsController::Status status,
                       int blocked_cookies) override;
  void OnBlockedCookiesCountChanged(int blocked_cookies) override;

 private:
  enum class IntermediateStep {
    kNone,
    // Show a button to disable cookie blocking on the current site.
    kTurnOffButton,
  };

  CookieControlsBubbleView(views::View* anchor_view,
                           content::WebContents* web_contents,
                           CookieControlsController* cookie_contols);
  ~CookieControlsBubbleView() override;

  void UpdateUi();

  // LocationBarBubbleDelegateView:
  void CloseBubble() override;
  int GetDialogButtons() const override;
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;
  void Init() override;
  base::string16 GetWindowTitle() const override;
  bool ShouldShowWindowTitle() const override;
  bool ShouldShowCloseButton() const override;
  void WindowClosing() override;
  bool Accept() override;
  bool Close() override;
  gfx::Size CalculatePreferredSize() const override;
  void AddedToWidget() override;

  // views::LinkListener:
  void LinkClicked(views::Link* source, int event_flags) override;

  CookieControlsController* controller_ = nullptr;

  CookieControlsController::Status status_ =
      CookieControlsController::Status::kUninitialized;

  IntermediateStep intermediate_step_ = IntermediateStep::kNone;

  base::Optional<int> blocked_cookies_;

  views::ImageView* header_view_ = nullptr;
  views::Label* text_ = nullptr;
  views::Link* not_working_link_ = nullptr;

  ScopedObserver<CookieControlsController, CookieControlsView> observer_{this};

  DISALLOW_COPY_AND_ASSIGN(CookieControlsBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_COOKIE_CONTROLS_BUBBLE_VIEW_H_
