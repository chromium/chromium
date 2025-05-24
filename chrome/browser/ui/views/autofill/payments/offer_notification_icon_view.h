// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_OFFER_NOTIFICATION_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_OFFER_NOTIFICATION_ICON_VIEW_H_

#include <string_view>

#include "base/timer/timer.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/widget/widget_observer.h"

class CommandUpdater;

namespace autofill {

class OfferNotificationBubbleController;

DECLARE_CUSTOM_ELEMENT_EVENT_TYPE(kLabelAnimationFinished);
DECLARE_CUSTOM_ELEMENT_EVENT_TYPE(kLabelExpansionFinished);

// The location bar icon to show the Offer bubble which displays any Credit Card
// related offers that are eligible on the current page domain.
class OfferNotificationIconView : public PageActionIconView,
                                  public views::WidgetObserver {
  METADATA_HEADER(OfferNotificationIconView, PageActionIconView)

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

  // WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  std::u16string_view GetIconLabelForTesting() const;

 protected:
  // PageActionIconView:
  void OnExecuting(PageActionIconView::ExecuteSource execute_source) override;
  void DidExecute(ExecuteSource execute_source) override;
  const gfx::VectorIcon& GetVectorIcon() const override;

 private:
  OfferNotificationBubbleController* GetController() const;

  // Observing the bubble view.
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      bubble_widget_observation_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_OFFER_NOTIFICATION_ICON_VIEW_H_
