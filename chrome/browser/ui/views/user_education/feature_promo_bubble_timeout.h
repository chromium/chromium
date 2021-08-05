// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_FEATURE_PROMO_BUBBLE_TIMEOUT_H_
#define CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_FEATURE_PROMO_BUBBLE_TIMEOUT_H_

#include "base/bind.h"
#include "base/macros.h"
#include "base/timer/timer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class FeaturePromoBubbleView;

// Handles the timeout for feature promo bubble. Closes the feature promo bubble
// when time outs.
class FeaturePromoBubbleTimeout {
 public:
  FeaturePromoBubbleTimeout(base::TimeDelta delay_no_interaction,
                            base::TimeDelta delay_after_interaction,
                            base::RepeatingClosure timeout_callback);
  ~FeaturePromoBubbleTimeout();

  void OnBubbleShown(FeaturePromoBubbleView* feature_promo_bubble_view);
  void OnMouseEntered();
  void OnMouseExited();

  // Initiates callback on timeout of the timer and closes the bubble.
  void OnTimeout();

 private:
  // Starts a timer to close the promo bubble.
  void StartAutoCloseTimer(base::TimeDelta auto_close_duration);

  // Timer used to auto close the bubble.
  base::OneShotTimer timer_;

  FeaturePromoBubbleView* feature_promo_bubble_view_;

  const base::TimeDelta delay_no_interaction_;
  const base::TimeDelta delay_after_interaction_;

  base::RepeatingClosure timeout_callback_;

  DISALLOW_COPY_AND_ASSIGN(FeaturePromoBubbleTimeout);
};

#endif  // CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_FEATURE_PROMO_BUBBLE_TIMEOUT_H_
