// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_VIRTUAL_CARD_ENROLL_BUBBLE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_VIRTUAL_CARD_ENROLL_BUBBLE_VIEWS_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/views/autofill/autofill_location_bar_bubble.h"
#include "chrome/browser/ui/views/autofill/payments/payments_view_util.h"
#include "components/autofill/core/browser/ui/payments/payments_bubble_closed_reasons.h"
#include "ui/views/controls/image_view.h"

namespace content {
class WebContents;
}

namespace autofill {

class VirtualCardEnrollBubbleController;

// This class handles the view for when users use a card that is also elligible
// to be enrolled in a virtual card.
class VirtualCardEnrollBubbleViews : public AutofillLocationBarBubble {
 public:
  // Bubble will be anchored to |anchor_view|.
  VirtualCardEnrollBubbleViews(views::View* anchor_view,
                               content::WebContents* web_contents,
                               VirtualCardEnrollBubbleController* controller);
  ~VirtualCardEnrollBubbleViews() override;
  VirtualCardEnrollBubbleViews(const VirtualCardEnrollBubbleViews&) = delete;
  VirtualCardEnrollBubbleViews& operator=(const VirtualCardEnrollBubbleViews&) =
      delete;

  void Show(DisplayReason reason);

  // AutofillBubbleBase:
  void Hide() override;

  // LocationBarBubbleDelegateView:
  void AddedToWidget() override;
  std::u16string GetWindowTitle() const override;
  void WindowClosing() override;

  views::View* GetLoadingProgressRowForTesting();

  void SwitchToLoadingState();

 protected:
  VirtualCardEnrollBubbleController* controller() const { return controller_; }

  // LocationBarBubbleDelegateView:
  void Init() override;

  bool OnDialogAccepted();
  void OnDialogDeclined();

 private:
  friend class VirtualCardEnrollBubbleViewsInteractiveUiTest;

  std::unique_ptr<views::View> CreateLegalMessageView();
  std::unique_ptr<views::View> CreateLoadingProgressRow();

  void LearnMoreLinkClicked();
  void GoogleLegalMessageClicked(const GURL& url);
  void IssuerLegalMessageClicked(const GURL& url);

  raw_ptr<VirtualCardEnrollBubbleController> controller_;

  // Container for the `loading_throbber_`.
  raw_ptr<views::View> loading_progress_row_ = nullptr;
  raw_ptr<views::Throbber> loading_throbber_ = nullptr;

  base::WeakPtrFactory<VirtualCardEnrollBubbleViews> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_VIRTUAL_CARD_ENROLL_BUBBLE_VIEWS_H_
