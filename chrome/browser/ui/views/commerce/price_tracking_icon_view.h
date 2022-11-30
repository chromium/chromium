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

  // IconLabelBubbleView:
  bool ShouldShowLabel() const override;

  void ForceVisibleForTesting(bool is_tracking_price);
  const std::u16string& GetIconLabelForTesting();

 protected:
  // PageActionIconView:
  void UpdateImpl() override;

 private:
  void EnablePriceTracking(bool enable);
  void SetVisualState(bool enable);
  void OnPriceTrackingServerStateUpdated(bool success);
  bool ShouldShow();
  bool IsPriceTracking() const;
  bool ShouldShowFirstUseExperienceBubble() const;

  raw_ptr<Profile> profile_;
  PriceTrackingBubbleCoordinator bubble_coordinator_;

  raw_ptr<const gfx::VectorIcon> icon_;
  std::u16string tooltip_text_and_accessibleName_;

  base::WeakPtrFactory<PriceTrackingIconView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_COMMERCE_PRICE_TRACKING_ICON_VIEW_H_
