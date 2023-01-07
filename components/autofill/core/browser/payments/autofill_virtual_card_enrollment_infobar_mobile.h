// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_VIRTUAL_CARD_ENROLLMENT_INFOBAR_MOBILE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_VIRTUAL_CARD_ENROLLMENT_INFOBAR_MOBILE_H_

#include <memory>

namespace infobars {
class InfoBar;
}

namespace autofill {

class AutofillVirtualCardEnrollmentInfoBarDelegateMobile;

// Creates an Infobar for saving a credit card on a mobile device.
std::unique_ptr<infobars::InfoBar> CreateVirtualCardEnrollmentInfoBarMobile(
    std::unique_ptr<AutofillVirtualCardEnrollmentInfoBarDelegateMobile>
        delegate);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_VIRTUAL_CARD_ENROLLMENT_INFOBAR_MOBILE_H_
