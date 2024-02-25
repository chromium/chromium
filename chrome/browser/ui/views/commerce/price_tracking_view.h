// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_COMMERCE_PRICE_TRACKING_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_COMMERCE_PRICE_TRACKING_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/commerce/core/shopping_service.h"
#include "components/commerce/core/subscriptions/subscriptions_observer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout_view.h"

class Profile;

class PriceTrackingView : public commerce::SubscriptionsObserver,
                          public views::FlexLayoutView {
  METADATA_HEADER(PriceTrackingView, views::FlexLayoutView)

 public:
  PriceTrackingView(Profile* profile,
                    const GURL& page_url,
                    bool is_price_track_enabled,
                    const commerce::ProductInfo& product_info);
  ~PriceTrackingView() override;

  bool IsToggleOn();

  // commerce::SubscriptionsObserver impl:
  void OnSubscribe(const commerce::CommerceSubscription& sub,
                   bool succeeded) override;
  void OnUnsubscribe(const commerce::CommerceSubscription& sub,
                     bool succeeded) override;

 private:
  friend class PriceTrackingViewTest;
  friend class PriceTrackingViewTestBase;

  std::u16string GetToggleAccessibleName();
  void OnToggleButtonPressed(const GURL& url);
  void UpdatePriceTrackingState(const GURL& url);
  void OnPriceTrackingStateUpdated(bool success);
  void HandleSubscriptionUpdate(const commerce::CommerceSubscription& sub,
                                bool is_tracking);

  raw_ptr<views::Label> body_label_;
  raw_ptr<views::ToggleButton> toggle_button_;

  raw_ptr<Profile> profile_;
  bool is_price_track_enabled_;
  commerce::ProductInfo product_info_;

  base::ScopedObservation<commerce::ShoppingService,
                          commerce::SubscriptionsObserver>
      scoped_observation_{this};

  base::WeakPtrFactory<PriceTrackingView> weak_ptr_factory_{this};
};
#endif  // CHROME_BROWSER_UI_VIEWS_COMMERCE_PRICE_TRACKING_VIEW_H_
