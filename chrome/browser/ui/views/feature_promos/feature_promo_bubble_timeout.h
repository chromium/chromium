// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FEATURE_PROMOS_FEATURE_PROMO_BUBBLE_TIMEOUT_H_
#define CHROME_BROWSER_UI_VIEWS_FEATURE_PROMOS_FEATURE_PROMO_BUBBLE_TIMEOUT_H_

#include "base/macros.h"
#include "base/timer/timer.h"

class FeaturePromoBubbleView;

// Handles the timeout for feature promo bubble. Closes the feature promo bubble
// when time outs.
class FeaturePromoBubbleTimeout {
 public:
  FeaturePromoBubbleTimeout(base::TimeDelta delay_default,
                            base::TimeDelta delay_short);

  void OnBubbleShown(FeaturePromoBubbleView* feature_promo_bubble_view);
  void OnMouseEntered();
  void OnMouseExited();

 private:
  // Starts a timer to close the promo bubble.
  void StartAutoCloseTimer(base::TimeDelta auto_close_duration);
  // Timer used to auto close the bubble.
  base::OneShotTimer timer_;

  FeaturePromoBubbleView* feature_promo_bubble_view_;

  const base::TimeDelta delay_default_;
  const base::TimeDelta delay_short_;

  DISALLOW_COPY_AND_ASSIGN(FeaturePromoBubbleTimeout);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FEATURE_PROMOS_FEATURE_PROMO_BUBBLE_TIMEOUT_H_
