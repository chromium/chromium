// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_SAVE_CARD_FAILURE_BUBBLE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_SAVE_CARD_FAILURE_BUBBLE_VIEWS_H_

#include "chrome/browser/ui/views/autofill/payments/save_card_bubble_views.h"

namespace autofill {

// This class displays the bubble shown when credit card upload failed.
class SaveCardFailureBubbleViews : public SaveCardBubbleViews {
 public:
  SaveCardFailureBubbleViews(views::View* anchor_view,
                             content::WebContents* web_contents,
                             SaveCardBubbleController* controller);

 protected:
  ~SaveCardFailureBubbleViews() override = default;

 private:
  // SaveCardBubbleViews:
  std::unique_ptr<views::View> CreateMainContentView() override;

  DISALLOW_COPY_AND_ASSIGN(SaveCardFailureBubbleViews);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_SAVE_CARD_FAILURE_BUBBLE_VIEWS_H_
