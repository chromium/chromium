// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_COMMERCE_PRICE_TRACKING_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_COMMERCE_PRICE_TRACKING_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout_view.h"

class PriceTrackingView : public views::FlexLayoutView {
 public:
  PriceTrackingView();

 private:
  raw_ptr<views::Label> body_label_;
  raw_ptr<views::ToggleButton> toggle_button_;
};
#endif  // CHROME_BROWSER_UI_VIEWS_COMMERCE_PRICE_TRACKING_VIEW_H_
