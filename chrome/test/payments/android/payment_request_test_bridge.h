// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_PAYMENTS_ANDROID_PAYMENT_REQUEST_TEST_BRIDGE_H_
#define CHROME_TEST_PAYMENTS_ANDROID_PAYMENT_REQUEST_TEST_BRIDGE_H_

#include "base/functional/callback.h"
#include "chrome/test/payments/payment_request_test_controller.h"

namespace content {
class WebContents;
}

namespace payments {

using SetAppDescriptionsCallback =
    base::RepeatingCallback<void(const std::vector<AppDescription>&)>;

// Sets a delegate on future Java PaymentRequests that returns the given values
// for queries about system state.
void SetUseDelegateOnPaymentRequestForTesting(
    bool is_incognito,
    bool is_valid_ssl,
    bool prefs_can_make_payment,
    const std::string& twa_package_name);

// Gets the WebContents of the Expandable Payment Handler for testing purpose,
// or null if nonexistent. To guarantee a non-null return, this function should
// be called only if: 1) PaymentRequest UI is opening. 2) PaymentHandler is
// opening.
content::WebContents* GetPaymentHandlerWebContentsForTest();

// Simulates a click on the security icon of the Payment Handler UI. Returns
// true on success.
bool ClickPaymentHandlerSecurityIconForTest();

// Click the close button on the Payment Handler UI. Returns true on success.
bool ClickPaymentHandlerCloseButtonForTest();

// Closes the payment dialog, if any. Returns true on success.
bool CloseDialogForTest();

// Clicks on the 'opt out' link in the SPC dialog, if available. Returns true on
// success, false if the opt out link wasn't being shown.
bool ClickSecurePaymentConfirmationOptOutForTest();

// Sets an observer on future Java PaymentRequests that will call these
// callbacks when the events occur.
void SetUseNativeObserverOnPaymentRequestForTesting(
    base::RepeatingClosure on_can_make_payment_called,
    base::RepeatingClosure on_can_make_payment_returned,
    base::RepeatingClosure on_has_enrolled_instrument_called,
    base::RepeatingClosure on_has_enrolled_instrument_returned,
    base::RepeatingClosure on_show_instruments_ready,
    SetAppDescriptionsCallback set_app_descriptions,
    base::RepeatingCallback<void(bool)> set_shipping_section_visible,
    base::RepeatingCallback<void(bool)> set_contact_section_visible,
    base::RepeatingClosure on_error_displayed,
    base::RepeatingClosure on_not_supported_error,
    base::RepeatingClosure on_connection_terminated,
    base::RepeatingClosure on_abort_called,
    base::RepeatingClosure on_complete_called,
    base::RepeatingClosure on_ui_displayed);

}  // namespace payments

#endif  // CHROME_TEST_PAYMENTS_ANDROID_PAYMENT_REQUEST_TEST_BRIDGE_H_
