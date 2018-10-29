// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_SAVE_CARD_OFFER_BUBBLE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_SAVE_CARD_OFFER_BUBBLE_VIEWS_H_

#include "chrome/browser/ui/views/autofill/save_card_bubble_views.h"

#include "chrome/browser/ui/views/autofill/view_util.h"
#include "ui/views/controls/styled_label_listener.h"
#include "ui/views/controls/textfield/textfield_controller.h"

namespace content {
class WebContents;
}

namespace autofill {

// This class displays the "Save credit card?" bubble that is shown when the
// user submits a form with a credit card number that Autofill has not
// previously saved. It includes a description of the card that is being saved
// and an [Save] button. (Non-material UI's include a [No Thanks] button).
class SaveCardOfferBubbleViews : public SaveCardBubbleViews,
                                 public views::StyledLabelListener,
                                 public views::TextfieldController {
 public:
  // Bubble will be anchored to |anchor_view|.
  SaveCardOfferBubbleViews(views::View* anchor_view,
                           const gfx::Point& anchor_point,
                           content::WebContents* web_contents,
                           SaveCardBubbleController* controller);

  // BubbleDialogDelegateView
  views::View* CreateFootnoteView() override;
  bool Accept() override;
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;
  bool IsDialogButtonEnabled(ui::DialogButton button) const override;

  // views::StyledLabelListener:
  void StyledLabelLinkClicked(views::StyledLabel* label,
                              const gfx::Range& range,
                              int event_flags) override;

  // views::TextfieldController:
  void ContentsChanged(views::Textfield* sender,
                       const base::string16& new_contents) override;

 private:
  std::unique_ptr<views::View> CreateMainContentView() override;

  ~SaveCardOfferBubbleViews() override;

  views::Textfield* cardholder_name_textfield_ = nullptr;

  LegalMessageView* legal_message_view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(SaveCardOfferBubbleViews);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_SAVE_CARD_OFFER_BUBBLE_VIEWS_H_
