// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_COOKIE_CONTROLS_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_COOKIE_CONTROLS_ICON_VIEW_H_

#include <memory>
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/browser/ui/cookie_controls/cookie_controls_controller.h"
#include "chrome/browser/ui/cookie_controls/cookie_controls_view.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls_bubble_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"

// View for the cookie control icon in the Omnibox.
class CookieControlsIconView : public PageActionIconView,
                               public CookieControlsView {
 public:
  explicit CookieControlsIconView(PageActionIconView::Delegate* delegate);
  ~CookieControlsIconView() override;

  // CookieControlsUI:
  void OnStatusChanged(CookieControlsController::Status status,
                       int blocked_cookies) override;
  void OnBlockedCookiesCountChanged(int blocked_cookies) override;

  // PageActionIconView:
  views::BubbleDialogDelegateView* GetBubble() const override;
  bool Update() override;
  base::string16 GetTextForTooltipAndAccessibleName() const override;

 protected:
  void OnExecuting(PageActionIconView::ExecuteSource source) override;
  const gfx::VectorIcon& GetVectorIcon() const override;

 private:
  bool HasAssociatedBubble() const;
  bool ShouldBeVisible() const;

  CookieControlsController::Status status_ =
      CookieControlsController::Status::kUninitialized;
  bool has_blocked_cookies_ = false;

  std::unique_ptr<CookieControlsController> controller_;
  ScopedObserver<CookieControlsController, CookieControlsView> observer_{this};

  DISALLOW_COPY_AND_ASSIGN(CookieControlsIconView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_COOKIE_CONTROLS_ICON_VIEW_H_
