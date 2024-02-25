// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_OFFER_NOTIFICATION_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_OFFER_NOTIFICATION_ICON_VIEW_H_

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

  const std::u16string& GetIconLabelForTesting() const;
  void SetAnimateOutTimerForTesting(base::RetainingOneShotTimer* timer);

 protected:
  // PageActionIconView:
  void OnExecuting(PageActionIconView::ExecuteSource execute_source) override;
  void DidExecute(ExecuteSource execute_source) override;
  const gfx::VectorIcon& GetVectorIcon() const override;

 private:
  OfferNotificationBubbleController* GetController() const;
  // IconLabelBubbleView:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

  // Show page action label if it meets the requirements.
  void MaybeShowPageActionLabel();
  // Hides the page action label.
  void HidePageActionLabel();
  base::RetainingOneShotTimer& AnimateOutTimer();
  // Animates out the price tracking icon label after a fixed period of time.
  // This keeps the label visible for long enough to give users an opportunity
  // to read the label text.
  base::RetainingOneShotTimer animate_out_timer_;
  raw_ptr<base::RetainingOneShotTimer> animate_out_timer_for_testing_ = nullptr;
  // Boolean that tracks whether we should extend the duration for which the
  // label is shown when it animates in.
  bool should_extend_label_shown_duration_ = false;
  // Observing the bubble view.
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      bubble_widget_observation_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_OFFER_NOTIFICATION_ICON_VIEW_H_
