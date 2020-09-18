// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/in_product_help/live_caption_promo_controller.h"

#include "chrome/browser/ui/in_product_help/live_caption_in_product_help.h"
#include "chrome/browser/ui/in_product_help/live_caption_in_product_help_factory.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/global_media_controls/media_toolbar_button_view.h"
#include "chrome/browser/ui/views/in_product_help/feature_promo_bubble_params.h"
#include "chrome/browser/ui/views/in_product_help/feature_promo_bubble_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/generated_resources.h"

LiveCaptionPromoController::LiveCaptionPromoController(
    BrowserView* browser_view)
    : browser_view_(browser_view) {}

LiveCaptionPromoController::~LiveCaptionPromoController() = default;

void LiveCaptionPromoController::ShowPromo() {
  FeaturePromoBubbleParams bubble_params;
  bubble_params.body_string_specifier = IDS_LIVE_CAPTION_PROMO;
  bubble_params.screenreader_string_specifier =
      IDS_LIVE_CAPTION_PROMO_SCREENREADER;
  bubble_params.anchor_view = browser_view_->toolbar()->media_button();
  bubble_params.arrow = views::BubbleBorder::Arrow::TOP_RIGHT;

  promo_bubble_ = FeaturePromoBubbleView::Create(std::move(bubble_params));
  promo_bubble_->set_close_on_deactivate(false);
  widget_observer_.Add(promo_bubble_->GetWidget());
  browser_view_->toolbar()->media_button()->AddObserver(this);
}

void LiveCaptionPromoController::PromoEnded() {
  LiveCaptionInProductHelpFactory::GetForProfile(
      browser_view_->browser()->profile())
      ->HelpDismissed();
}

void LiveCaptionPromoController::OnMediaDialogOpened() {
  promo_bubble_->GetWidget()->CloseNow();
  PromoEnded();
}

void LiveCaptionPromoController::OnMediaButtonDisabled() {
  promo_bubble_->GetWidget()->CloseNow();
}

void LiveCaptionPromoController::OnWidgetDestroying(views::Widget* widget) {
  DCHECK(promo_bubble_);
  promo_bubble_ = nullptr;
  browser_view_->toolbar()->media_button()->RemoveObserver(this);
  widget_observer_.Remove(widget);
}
