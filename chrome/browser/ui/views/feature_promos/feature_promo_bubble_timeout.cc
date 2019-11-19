// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/feature_promos/feature_promo_bubble_timeout.h"

#include <memory>

#include "chrome/browser/ui/views/feature_promos/feature_promo_bubble_view.h"

FeaturePromoBubbleTimeout::FeaturePromoBubbleTimeout(
    base::TimeDelta delay_default,
    base::TimeDelta delay_short)
    : delay_default_(delay_default), delay_short_(delay_short) {}

void FeaturePromoBubbleTimeout::OnBubbleShown(
    FeaturePromoBubbleView* feature_promo_bubble_view) {
  feature_promo_bubble_view_ = feature_promo_bubble_view;
  if (delay_default_.is_zero())
    return;
  StartAutoCloseTimer(delay_default_);
}

void FeaturePromoBubbleTimeout::OnMouseEntered() {
  timer_.Stop();
}

void FeaturePromoBubbleTimeout::OnMouseExited() {
  if (delay_short_.is_zero() && delay_default_.is_zero())
    return;

  if (delay_short_.is_zero())
    StartAutoCloseTimer(delay_default_);
  else
    StartAutoCloseTimer(delay_short_);
}

void FeaturePromoBubbleTimeout::StartAutoCloseTimer(
    base::TimeDelta auto_close_duration) {
  timer_.Start(FROM_HERE, auto_close_duration, feature_promo_bubble_view_,
               &FeaturePromoBubbleView::CloseBubble);
}
