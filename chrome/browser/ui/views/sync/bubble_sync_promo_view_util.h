// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SYNC_BUBBLE_SYNC_PROMO_VIEW_UTIL_H_
#define CHROME_BROWSER_UI_VIEWS_SYNC_BUBBLE_SYNC_PROMO_VIEW_UTIL_H_

#include <memory>

#include "build/build_config.h"
#include "components/signin/public/base/signin_metrics.h"
#include "ui/views/style/typography.h"

namespace views {
class View;
}

class BubbleSyncPromoDelegate;
class Profile;

// Parameters for CreateBubbleSyncPromoView.
struct BubbleSyncPromoViewParams {
  // Used when Dice is not enabled.
  int link_text_resource_id = 0;
  int message_text_resource_id = 0;

  // Used when Dice is enabled.
  int dice_no_accounts_promo_message_resource_id = 0;
  int dice_accounts_promo_message_resource_id = 0;
  bool dice_signin_button_prominent = true;
  int dice_text_style = views::style::STYLE_PRIMARY;
};

// Creates a view that can be used a a sync promo. ChromeOS does not have sync
// promos.
std::unique_ptr<views::View> CreateBubbleSyncPromoView(
    Profile* profile,
    BubbleSyncPromoDelegate* delegate,
    signin_metrics::AccessPoint access_point,
    const BubbleSyncPromoViewParams& params);

#endif  // CHROME_BROWSER_UI_VIEWS_SYNC_BUBBLE_SYNC_PROMO_VIEW_UTIL_H_
