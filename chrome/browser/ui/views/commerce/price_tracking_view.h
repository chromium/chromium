// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_COMMERCE_PRICE_TRACKING_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_COMMERCE_PRICE_TRACKING_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout_view.h"

class Profile;

class PriceTrackingView : public views::FlexLayoutView {
 public:
  PriceTrackingView(Profile* profile,
                    const GURL& page_url,
                    const gfx::ImageSkia& product_image,
                    bool is_price_track_enabled);
  ~PriceTrackingView() override;

  bool IsToggleOn();

 private:
  friend class PriceTrackingViewTest;

  std::u16string GetToggleAccessibleName();
  void OnToggleButtonPressed(const GURL& url);
  void UpdatePriceTrackingState(const GURL& url);
  void OnPriceTrackingStateUpdated(bool success);

  raw_ptr<views::Label> body_label_;
  raw_ptr<views::ToggleButton> toggle_button_;

  raw_ptr<Profile> profile_;
  bool is_price_track_enabled_;

  base::WeakPtrFactory<PriceTrackingView> weak_ptr_factory_{this};
};
#endif  // CHROME_BROWSER_UI_VIEWS_COMMERCE_PRICE_TRACKING_VIEW_H_
