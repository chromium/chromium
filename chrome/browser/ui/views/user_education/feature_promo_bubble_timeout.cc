// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/feature_promo_bubble_timeout.h"

#include <memory>

#include "base/bind.h"
#include "chrome/browser/ui/views/user_education/feature_promo_bubble_view.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

FeaturePromoBubbleTimeout::FeaturePromoBubbleTimeout(
    base::TimeDelta delay_no_interaction,
    base::TimeDelta delay_after_interaction,
    base::RepeatingClosure timeout_callback)
    : delay_no_interaction_(delay_no_interaction),
      delay_after_interaction_(delay_after_interaction),
      timeout_callback_(timeout_callback) {}
FeaturePromoBubbleTimeout::~FeaturePromoBubbleTimeout() = default;

void FeaturePromoBubbleTimeout::OnBubbleShown(
    FeaturePromoBubbleView* feature_promo_bubble_view) {
  feature_promo_bubble_view_ = feature_promo_bubble_view;
  if (delay_no_interaction_.is_zero())
    return;
  StartAutoCloseTimer(delay_no_interaction_);
}

void FeaturePromoBubbleTimeout::OnMouseEntered() {
  timer_.Stop();
}

void FeaturePromoBubbleTimeout::OnMouseExited() {
  if (delay_after_interaction_.is_zero() && delay_no_interaction_.is_zero())
    return;

  if (delay_after_interaction_.is_zero())
    StartAutoCloseTimer(delay_no_interaction_);
  else
    StartAutoCloseTimer(delay_after_interaction_);
}

void FeaturePromoBubbleTimeout::StartAutoCloseTimer(
    base::TimeDelta auto_close_duration) {
  timer_.Start(FROM_HERE, auto_close_duration, this,
               &FeaturePromoBubbleTimeout::OnTimeout);
}

void FeaturePromoBubbleTimeout::OnTimeout() {
  if (timeout_callback_)
    timeout_callback_.Run();
  feature_promo_bubble_view_->CloseBubble();
}
