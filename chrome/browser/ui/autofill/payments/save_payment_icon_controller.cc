// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/save_payment_icon_controller.h"
#include "chrome/browser/ui/autofill/payments/save_card_bubble_controller_impl.h"

namespace autofill {

// static
SavePaymentIconController* SavePaymentIconController::GetOrCreate(
    content::WebContents* web_contents) {
  if (!web_contents)
    return nullptr;

  SaveCardBubbleControllerImpl::CreateForWebContents(web_contents);
  return SaveCardBubbleControllerImpl::FromWebContents(web_contents);
}

// static
SavePaymentIconController* SavePaymentIconController::Get(
    content::WebContents* web_contents) {
  if (!web_contents)
    return nullptr;

  return SaveCardBubbleControllerImpl::FromWebContents(web_contents);
}

}  // namespace autofill
