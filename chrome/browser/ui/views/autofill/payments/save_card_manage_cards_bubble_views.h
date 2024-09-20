// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_SAVE_CARD_MANAGE_CARDS_BUBBLE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_SAVE_CARD_MANAGE_CARDS_BUBBLE_VIEWS_H_

#include "chrome/browser/ui/views/autofill/payments/save_card_bubble_views.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace autofill {

// This class displays the Manage cards bubble that is shown after the user
// submits a form with a credit card number that Autofill has not
// previously saved and chooses to save it. This bubble is accessible by
// clicking on the omnibox credit card icon. It contains a description of the
// credit card that was just saved, a [Manage cards] button that links to the
// Autofill settings page, and a [Done] button that closes the bubble.
class SaveCardManageCardsBubbleViews : public SaveCardBubbleViews {
  METADATA_HEADER(SaveCardManageCardsBubbleViews, SaveCardBubbleViews)
 public:
  // Bubble will be anchored to |anchor_view|.
  SaveCardManageCardsBubbleViews(views::View* anchor_view,
                                 content::WebContents* web_contents,
                                 SaveCardBubbleController* controller);

  SaveCardManageCardsBubbleViews(const SaveCardManageCardsBubbleViews&) =
      delete;
  SaveCardManageCardsBubbleViews& operator=(
      const SaveCardManageCardsBubbleViews&) = delete;
  ~SaveCardManageCardsBubbleViews() override;

 private:
  std::unique_ptr<views::View> CreateMainContentView() override;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_SAVE_CARD_MANAGE_CARDS_BUBBLE_VIEWS_H_
