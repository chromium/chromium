// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_SAVE_PAYMENT_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_SAVE_PAYMENT_ICON_VIEW_H_

#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

class CommandUpdater;

namespace autofill {

class SavePaymentIconController;

// The location bar icon to show the Save Credit Card bubble where the user can
// choose to save the credit card info to use again later without re-entering
// it.
class SavePaymentIconView : public PageActionIconView {
  METADATA_HEADER(SavePaymentIconView, PageActionIconView)

 public:
  SavePaymentIconView(CommandUpdater* command_updater,
                      IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
                      PageActionIconView::Delegate* page_action_icon_delegate,
                      int command_id);

  SavePaymentIconView(const SavePaymentIconView&) = delete;
  SavePaymentIconView& operator=(const SavePaymentIconView&) = delete;

  ~SavePaymentIconView() override;

  // PageActionIconView:
  views::BubbleDialogDelegate* GetBubble() const override;
  void UpdateImpl() override;
  std::u16string GetTextForTooltipAndAccessibleName() const override;

 protected:
  // PageActionIconView:
  void OnExecuting(PageActionIconView::ExecuteSource execute_source) override;
  const gfx::VectorIcon& GetVectorIcon() const override;

 private:
  SavePaymentIconController* GetController() const;

  // gfx::AnimationDelegate:
  void AnimationEnded(const gfx::Animation* animation) override;

  int command_id_ = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_SAVE_PAYMENT_ICON_VIEW_H_
