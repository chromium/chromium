// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_COMMERCE_DISCOUNTS_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_COMMERCE_DISCOUNTS_ICON_VIEW_H_

#include "base/timer/timer.h"
#include "chrome/browser/ui/views/commerce/discounts_bubble_dialog_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace commerce {
class CommerceUiTabHelper;
}  // namespace commerce

namespace gfx {
struct VectorIcon;
}  // namespace gfx

class DiscountsIconView : public PageActionIconView {
  METADATA_HEADER(DiscountsIconView, PageActionIconView)

 public:
  DiscountsIconView(IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
                    PageActionIconView::Delegate* page_action_icon_delegate);
  ~DiscountsIconView() override;

  // PageActionIconView:
  views::BubbleDialogDelegate* GetBubble() const override;

 protected:
  // PageActionIconView:
  void OnExecuting(PageActionIconView::ExecuteSource execute_source) override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  void UpdateImpl() override;

 private:
  // IconLabelBubbleView:
  void AnimationProgressed(const gfx::Animation* animation) override;

  bool ShouldShow();
  void HidePageActionLabel();
  void MaybeShowPageActionLabel();
  commerce::CommerceUiTabHelper* GetTabHelper();
  void MaybeShowBubble(bool from_user);

  DiscountsBubbleCoordinator bubble_coordinator_;

  // Boolean that tracks whether we should extend the duration for which the
  // label is shown when it animates in.
  bool should_extend_label_shown_duration_ = false;
  // Animates out the discounts icon label after a fixed period of time.
  // This keeps the label visible for long enough to give users an opportunity
  // to read the label text.
  base::OneShotTimer animate_out_timer_;

  base::WeakPtrFactory<DiscountsIconView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_COMMERCE_DISCOUNTS_ICON_VIEW_H_
