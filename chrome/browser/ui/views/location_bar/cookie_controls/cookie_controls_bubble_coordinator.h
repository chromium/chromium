// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_COOKIE_CONTROLS_COOKIE_CONTROLS_BUBBLE_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_COOKIE_CONTROLS_COOKIE_CONTROLS_BUBBLE_COORDINATOR_H_

#include "base/memory/raw_ptr.h"
#include "components/content_settings/browser/ui/cookie_controls_controller.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/view_observer.h"

class CookieControlsBubbleViewController;

namespace content {
class WebContents;
}

class CookieControlsBubbleCoordinator : public views::ViewObserver {
 public:
  ~CookieControlsBubbleCoordinator() override;

  explicit CookieControlsBubbleCoordinator(views::View* anchor_view);

  void ShowBubble(content::WebContents* web_contents,
                  content_settings::CookieControlsController* controller);

 private:
  // views::ViewObserver
  void OnViewIsDeleting(views::View* observed_view) override;

  raw_ptr<views::View> anchor_view_;
  std::unique_ptr<CookieControlsBubbleViewController> view_controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_COOKIE_CONTROLS_COOKIE_CONTROLS_BUBBLE_COORDINATOR_H_
