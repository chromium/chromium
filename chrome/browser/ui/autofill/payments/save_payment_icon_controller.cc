// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/save_payment_icon_controller.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/autofill/payments/iban_bubble_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/save_card_bubble_controller_impl.h"

namespace autofill {

// static
SavePaymentIconController* SavePaymentIconController::Get(
    content::WebContents* web_contents,
    int command_id) {
  if (!web_contents)
    return nullptr;

  if (command_id == IDC_SAVE_CREDIT_CARD_FOR_PAGE) {
    return SaveCardBubbleControllerImpl::FromWebContents(web_contents);
  }
  DCHECK_EQ(command_id, IDC_SAVE_IBAN_FOR_PAGE);
  return IbanBubbleControllerImpl::FromWebContents(web_contents);
}

}  // namespace autofill
