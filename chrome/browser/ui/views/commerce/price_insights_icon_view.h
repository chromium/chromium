// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_COMMERCE_PRICE_INSIGHTS_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_COMMERCE_PRICE_INSIGHTS_ICON_VIEW_H_

#include "base/timer/timer.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/vector_icon_types.h"

class Profile;

// This icon appears in the location bar when the current page qualifies for
// price insight information. Upon clicking, it opens the side panel with more
// price information.
class PriceInsightsIconView : public PageActionIconView {
 public:
  METADATA_HEADER(PriceInsightsIconView);
  PriceInsightsIconView(
      IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
      PageActionIconView::Delegate* page_action_icon_delegate,
      Profile* profile);
  ~PriceInsightsIconView() override;

  // Enum for logging the price insights icon label. Each label we ever use
  // should have a separate enum even if they are semantically similar (e.g.
  // "Price is low" vs. "Great price") since it could have a nontrivial effect
  // on the click-through rate. These values are persisted to logs. Entries
  // should not be renumbered and numeric values should never be reused.
  enum class PriceInsightsIconLabelType {
    kNone = 0,
    kPriceIsLow = 1,
    kPriceIsHigh = 2,
    kMaxValue = kPriceIsHigh,
  };

  // PageActionIconView:
  views::BubbleDialogDelegate* GetBubble() const override;

  const std::u16string& GetIconLabelForTesting();

  bool IsIconHighlightedForTesting();

 protected:
  // PageActionIconView:
  const gfx::VectorIcon& GetVectorIcon() const override;
  void UpdateImpl() override;
  void OnExecuting(PageActionIconView::ExecuteSource execute_source) override;

 private:
  // IconLabelBubbleView:
  void AnimationProgressed(const gfx::Animation* animation) override;

  // Return whether the icon need to be shown.
  bool ShouldShow() const;

  // Show page action label if it meets the feature engagement requirements.
  void MaybeShowPageActionLabel();

  // Gets the label type from the commerce tab helper. This is a proxy method
  // for ShoppingListUiTabHelper::GetPriceInsightsIconLabelTypeForPage.
  PriceInsightsIconView::PriceInsightsIconLabelType GetLabelTypeForPage();

  // Update the label for the page action based on the last known label type.
  void UpdatePriceInsightsIconLabel();

  // Hides the page action label.
  void HidePageActionLabel();

  const raw_ptr<Profile> profile_;
  raw_ptr<const gfx::VectorIcon> icon_;

  // Animates out the price tracking icon label after a fixed period of time.
  // This keeps the label visible for long enough to give users an opportunity
  // to read the label text.
  base::RetainingOneShotTimer animate_out_timer_;
  // Boolean that tracks whether we should extend the duration for which the
  // label is shown when it animates in.
  bool should_extend_label_shown_duration_ = false;
};

#endif  // CHROME_BROWSER_UI_VIEWS_COMMERCE_PRICE_INSIGHTS_ICON_VIEW_H_
