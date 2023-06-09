// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_COMMERCE_PRICE_INSIGHTS_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_COMMERCE_PRICE_INSIGHTS_ICON_VIEW_H_

#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/vector_icon_types.h"

// This icon appears in the location bar when the current page qualifies for
// price insight information. Upon clicking, it opens the side panel with more
// price information.
class PriceInsightsIconView : public PageActionIconView {
 public:
  METADATA_HEADER(PriceInsightsIconView);
  PriceInsightsIconView(
      IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
      PageActionIconView::Delegate* page_action_icon_delegate);
  ~PriceInsightsIconView() override;

  // PageActionIconView:
  views::BubbleDialogDelegate* GetBubble() const override;

 protected:
  // PageActionIconView:
  const gfx::VectorIcon& GetVectorIcon() const override;
  void UpdateImpl() override;
  void OnExecuting(PageActionIconView::ExecuteSource execute_source) override;

 private:
  bool ShouldShow() const;

  raw_ptr<const gfx::VectorIcon> icon_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_COMMERCE_PRICE_INSIGHTS_ICON_VIEW_H_
