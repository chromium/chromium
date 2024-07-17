// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_COMMERCE_DISCOUNTS_BUBBLE_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_COMMERCE_DISCOUNTS_BUBBLE_DIALOG_VIEW_H_

#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/commerce/discounts_coupon_code_label_view.h"
#include "chrome/browser/ui/views/controls/page_switcher_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "components/commerce/core/commerce_types.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/view_tracker.h"
#include "ui/views/widget/widget_observer.h"

namespace content {
class WebContents;
}  // namespace content

DECLARE_ELEMENT_IDENTIFIER_VALUE(kDiscountsBubbleDialogId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kDiscountsBubbleMainPageId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kDiscountsBubbleTermsAndConditionLabelId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kDiscountsBubbleTermsAndConditionPageId);

class DiscountsBubbleDialogView : public LocationBarBubbleDelegateView {
  METADATA_HEADER(DiscountsBubbleDialogView, LocationBarBubbleDelegateView)
 public:
  DiscountsBubbleDialogView(View* anchor_view,
                            content::WebContents* web_contents,
                            const commerce::DiscountInfo& discount_info);
  ~DiscountsBubbleDialogView() override;

  // view
  void AddedToWidget() override;

 private:
  void OpenMainPage(commerce::DiscountInfo discount_info,
                    std::string seller_domain);
  std::unique_ptr<views::View> CreateMainPageHeaderView();
  std::unique_ptr<views::View> CreateMainPageTitleView(
      const commerce::DiscountInfo& discount_info);
  std::unique_ptr<views::View> CreateMainPageContent(
      const commerce::DiscountInfo& discount_info,
      const std::string& seller_domain);

  void OpenTermsAndConditionsPage(commerce::DiscountInfo discount_info,
                                  std::string seller_domain);

  void CopyButtonClicked();
  void OnDialogClosing();

  raw_ptr<PageSwitcherView> page_container_ = nullptr;

  const commerce::DiscountInfo discount_info_;
  ukm::SourceId ukm_source_id_;

  base::WeakPtrFactory<DiscountsBubbleDialogView> weak_factory_{this};
};

class DiscountsBubbleCoordinator : public views::WidgetObserver {
 public:
  explicit DiscountsBubbleCoordinator(views::View* anchor_view);
  ~DiscountsBubbleCoordinator() override;

  // WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  void Show(content::WebContents* web_contents,
            const commerce::DiscountInfo& discount_info,
            base::OnceClosure on_dialog_closing_callback);
  void Hide();
  DiscountsBubbleDialogView* GetBubble() const;

 private:
  bool IsShowing();

  const raw_ptr<views::View> anchor_view_;
  views::ViewTracker tracker_;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      bubble_widget_observation_{this};

  base::OnceClosure on_dialog_closing_callback_;
};
#endif  // CHROME_BROWSER_UI_VIEWS_COMMERCE_DISCOUNTS_BUBBLE_DIALOG_VIEW_H_
