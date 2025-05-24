// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_ADDRESS_SIGN_IN_PROMO_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_ADDRESS_SIGN_IN_PROMO_VIEW_H_

#include "chrome/browser/ui/views/autofill/address_bubble_base_view.h"

namespace autofill {

class AutofillProfile;

// This is the sign in promo view that is shown after a user accepted the
// address save/update bubble without being signed into Chrome.
class AddressSignInPromoView : public AddressBubbleBaseView {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kBubbleFrameViewId);

  explicit AddressSignInPromoView(views::View* anchor_view,
                                  content::WebContents* web_contents,
                                  const AutofillProfile& autofill_profile);

  AddressSignInPromoView(const AddressSignInPromoView&) = delete;
  AddressSignInPromoView& operator=(const AddressSignInPromoView&) = delete;
  ~AddressSignInPromoView() override;

  // View:
  void AddedToWidget() override;

  // AutofillBubbleBase:
  void Hide() override;

  // views::WidgetDelegate:
  void WindowClosing() override;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_ADDRESS_SIGN_IN_PROMO_VIEW_H_
