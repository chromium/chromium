// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_OFFER_NOTIFICATION_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_OFFER_NOTIFICATION_ICON_VIEW_H_

#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"

class CommandUpdater;

namespace autofill {

class OfferNotificationBubbleController;

// The location bar icon to show the Offer bubble which displays any Credit Card
// related offers that are eligible on the current page domain.
class OfferNotificationIconView : public PageActionIconView {
 public:
  OfferNotificationIconView(
      CommandUpdater* command_updater,
      IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
      PageActionIconView::Delegate* page_action_icon_delegate);
  ~OfferNotificationIconView() override;
  OfferNotificationIconView(const OfferNotificationIconView&) = delete;
  OfferNotificationIconView& operator=(const OfferNotificationIconView&) =
      delete;

  // PageActionIconView:
  views::BubbleDialogDelegate* GetBubble() const override;
  void UpdateImpl() override;
  std::u16string GetTextForTooltipAndAccessibleName() const override;

 protected:
  // PageActionIconView:
  void OnExecuting(PageActionIconView::ExecuteSource execute_source) override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  const char* GetClassName() const override;

 private:
  OfferNotificationBubbleController* GetController() const;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_OFFER_NOTIFICATION_ICON_VIEW_H_
