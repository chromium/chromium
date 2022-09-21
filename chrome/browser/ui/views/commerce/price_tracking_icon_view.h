// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_COMMERCE_PRICE_TRACKING_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_COMMERCE_PRICE_TRACKING_ICON_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/commerce/price_tracking_bubble_dialog_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "ui/gfx/vector_icon_types.h"

class Profile;

// This icon appears in the location bar when the current page qualifies for
// price tracking. Upon clicking, it shows a bubble where the user can choose to
// track or untrack the current page.
class PriceTrackingIconView : public PageActionIconView {
 public:
  PriceTrackingIconView(IconLabelBubbleView::Delegate* parent_delegate,
                        Delegate* delegate,
                        Profile* profile);
  ~PriceTrackingIconView() override;

  // PageActionIconView:
  views::BubbleDialogDelegate* GetBubble() const override;
  std::u16string GetTextForTooltipAndAccessibleName() const override;
  void OnExecuting(PageActionIconView::ExecuteSource execute_source) override;
  const gfx::VectorIcon& GetVectorIcon() const override;

  void ForceVisibleForTesting(bool is_tracking_price);
  const std::u16string& GetIconLabelForTesting();
  const gfx::VectorIcon* GetVectorIconForTesting();

 protected:
  // PageActionIconView:
  void UpdateImpl() override;

 private:
  void UpdatePriceTrackingState(bool enable);
  void OnPriceTrackingStateUpdated(bool success);
  bool IsPriceTracking() const;
  void ResetForceMode();

  raw_ptr<Profile> profile_;
  PriceTrackingBubbleCoordinator bubble_coordinator_;
  // This is used to update this icon view immediately without waiting for the
  // server response. This is also used in test.
  bool force_mode_ = false;
  bool is_tracking_price_ = false;
  // This is needed for test to verify the current icon because |force_mode_|
  // need to reset immediately after UpdateImpl() which might change the result
  // of GetVectorIcon().
  const gfx::VectorIcon* vector_icon_for_testing_;

  // Currently, these are being set by test.
  bool is_visible_ = false;

  base::WeakPtrFactory<PriceTrackingIconView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_COMMERCE_PRICE_TRACKING_ICON_VIEW_H_
