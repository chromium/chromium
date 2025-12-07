// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_PAYMENTS_VIEW_FACTORY_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_PAYMENTS_VIEW_FACTORY_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"

namespace content {
class WebContents;
}

namespace autofill {

class AutofillErrorDialogController;
class AutofillErrorDialogView;
class AutofillProgressDialogController;
class AutofillProgressDialogView;
class BnplTosController;
class BnplTosView;
class CardUnmaskAuthenticationSelectionDialogController;
class CardUnmaskAuthenticationSelectionDialog;
class CardUnmaskOtpInputDialogController;
class CardUnmaskOtpInputDialogView;
class SaveAndFillDialogController;
class SaveAndFillDialogView;

namespace payments {
class PaymentsWindowUserConsentDialogController;
class PaymentsWindowUserConsentDialog;
class SelectBnplIssuerView;
class SelectBnplIssuerDialogController;
}  // namespace payments

// Factory function for creating and showing the autofill progress dialog
// view.
// Note: On Desktop the view's ownership is transferred to the widget, which
// deletes it on dismissal, so no lifecycle management is needed. However, on
// Android this is not the case, the view's implementation must delete itself
// when dismissed.
std::unique_ptr<AutofillProgressDialogView> CreateAndShowProgressDialog(
    base::WeakPtr<AutofillProgressDialogController> controller,
    content::WebContents* web_contents);

// Factory function for creating and showing the view.
base::WeakPtr<AutofillErrorDialogView> CreateAndShowAutofillErrorDialog(
    AutofillErrorDialogController* controller,
    content::WebContents* web_contents);

// Factory function for the card unmask view creates and shows the dialog.
CardUnmaskAuthenticationSelectionDialog*
CreateAndShowCardUnmaskAuthenticationSelectionDialog(
    content::WebContents* web_contents,
    CardUnmaskAuthenticationSelectionDialogController* controller);

// Factory function for creating and showing the card unmask otp input dialog
// view. This dialog is shown when user needs to input the OTP text received
// to unmask the credit card.
base::WeakPtr<CardUnmaskOtpInputDialogView> CreateAndShowOtpInputDialog(
    base::WeakPtr<CardUnmaskOtpInputDialogController> controller,
    content::WebContents* web_contents);

// Factory function for creating the payments window user consent dialog. This
// dialog is triggered when a payments window flow is started and if there was
// no intentional user consent to open a pop-up and redirect to a payments
// window flow. Once the dialog is accepted, it is treated as receiving user
// consent and the payments window flow will start. If the dialog is cancelled,
// the flow will end.
base::WeakPtr<payments::PaymentsWindowUserConsentDialog>
CreateAndShowPaymentsWindowUserConsentDialog(
    base::WeakPtr<payments::PaymentsWindowUserConsentDialogController>
        controller,
    content::WebContents* web_contents,
    base::OnceClosure accept_callback,
    base::OnceClosure cancel_callback);

// Factory function for creating and showing the BNPL Terms of Service.
std::unique_ptr<BnplTosView> CreateAndShowBnplTos(
    base::WeakPtr<BnplTosController> controller,
    content::WebContents* web_contents);

#if !BUILDFLAG(IS_ANDROID)
// Factory function for creating the "Save and Fill" dialog. This dialog
// is triggered when the user has no saved credit cards and clicks on the
// "Save and Fill" suggestion in the credit card dropdown menu. It presents
// a centered modal dialog where the user can conveniently save a new
// credit card and simultaneously fill it into the form with a single click.
std::unique_ptr<SaveAndFillDialogView> CreateAndShowSaveAndFillDialog(
    base::WeakPtr<SaveAndFillDialogController> controller,
    content::WebContents* web_contents);
#endif  // !BUILDFLAG(IS_ANDROID)

// Factory function for creating and showing the BNPL issuer selection dialog.
// This dialog is triggered when the BNPL payment method has been selected and
// the user needs to select an issuer.
// `has_seen_ai_terms` indicates whether the user has seen the amount extraction
// AI terms. In the AI-based amount extraction case, if the user who clicked on
// the payment form has seen the AI terms, the throbber has to be shown first
// while the server-side AI is inferencing the final checkout amount.
std::unique_ptr<payments::SelectBnplIssuerView>
CreateAndShowBnplIssuerSelectionDialog(
    base::WeakPtr<payments::SelectBnplIssuerDialogController> controller,
    content::WebContents* web_contents,
    bool has_seen_ai_terms);

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_PAYMENTS_VIEW_FACTORY_H_
