// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_IN_PRODUCT_HELP_LIVE_CAPTION_PROMO_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_IN_PRODUCT_HELP_LIVE_CAPTION_PROMO_CONTROLLER_H_

#include "base/scoped_observer.h"
#include "chrome/browser/ui/global_media_controls/media_toolbar_button_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

class BrowserView;
class FeaturePromoBubbleView;

// Handles display of the live caption in-product help promo. Notifies the
// |LiveCaptionInProductHelp| service when the promo is finished.
class LiveCaptionPromoController : public MediaToolbarButtonObserver,
                                   public views::WidgetObserver {
 public:
  explicit LiveCaptionPromoController(BrowserView* browser_view);
  ~LiveCaptionPromoController() override;
  LiveCaptionPromoController(const LiveCaptionPromoController&) = delete;
  LiveCaptionPromoController& operator=(const LiveCaptionPromoController&) =
      delete;

  // Shows the IPH promo. Should only be called once.
  void ShowPromo();

 private:
  friend class LiveCaptionPromoControllerTest;

  // MediaToolbarButtonObserver:
  void OnMediaDialogOpened() override;
  void OnMediaButtonShown() override {}
  void OnMediaButtonHidden() override {}
  void OnMediaButtonEnabled() override {}
  void OnMediaButtonDisabled() override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // Called when the promo flow ends.
  void PromoEnded();

  BrowserView* const browser_view_;
  FeaturePromoBubbleView* promo_bubble_ = nullptr;
  ScopedObserver<views::Widget, views::WidgetObserver> widget_observer_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_IN_PRODUCT_HELP_LIVE_CAPTION_PROMO_CONTROLLER_H_
