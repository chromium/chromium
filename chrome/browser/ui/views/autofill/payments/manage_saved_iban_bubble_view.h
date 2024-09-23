// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_MANAGE_SAVED_IBAN_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_MANAGE_SAVED_IBAN_BUBBLE_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/payments/iban_bubble_controller.h"
#include "chrome/browser/ui/views/autofill/autofill_location_bar_bubble.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace autofill {

// This class displays the manage saved IBAN bubble that is shown after the user
// submits a form with an IBAN value that has been saved. This bubble is
// accessible by clicking on the omnibox IBAN icon.
class ManageSavedIbanBubbleView : public AutofillLocationBarBubble {
  METADATA_HEADER(ManageSavedIbanBubbleView, AutofillLocationBarBubble)
 public:
  // Bubble will be anchored to `anchor_view`.
  ManageSavedIbanBubbleView(views::View* anchor_view,
                            content::WebContents* web_contents,
                            IbanBubbleController* controller);
  ManageSavedIbanBubbleView(const ManageSavedIbanBubbleView&) = delete;
  ManageSavedIbanBubbleView& operator=(const ManageSavedIbanBubbleView&) =
      delete;
  ~ManageSavedIbanBubbleView() override;

  void Show(DisplayReason reason);

  // AutofillBubbleBase:
  void Hide() override;

  // LocationBarBubbleDelegateView:
  std::u16string GetWindowTitle() const override;
  void WindowClosing() override;

 private:

  IbanBubbleController* controller() const { return controller_; }

  // Attributes IDs to the dialog's DialogDelegate-supplied buttons. This is
  // needed when the browser tries to find the view by ID and clicks on it.
  void AssignIdsToDialogButtons();

  // LocationBarBubbleDelegateView:
  // Creates the main content for the manage saved IBAN view, which includes
  // IBAN value and nickname if given.
  void Init() override;

  // For IBAN manage bubble view where the user can view the IBAN nickname if
  // available. It will be non-nullptr if the user inputs a nickname when
  // saving the IBAN in the IBAN save bubble.
  raw_ptr<views::Label> nickname_label_ = nullptr;

  raw_ptr<IbanBubbleController> controller_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_MANAGE_SAVED_IBAN_BUBBLE_VIEW_H_
