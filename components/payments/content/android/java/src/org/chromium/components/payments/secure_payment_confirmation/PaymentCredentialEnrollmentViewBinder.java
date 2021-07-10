// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.secure_payment_confirmation;

import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * The view binder of the SecurePaymentConfirmation Enrollment UI, which is stateless. It is called
 * to bind a given model to a given view. Should contain as little business logic as possible.
 */
/* package */ class PaymentCredentialEnrollmentViewBinder {
    /* package */ static void bind(
            PropertyModel model, PaymentCredentialEnrollmentView view, PropertyKey propertyKey) {
        if (PaymentCredentialEnrollmentProperties.PAYMENT_ICON == propertyKey) {
            view.mPaymentIcon.setImageDrawable(
                    model.get(PaymentCredentialEnrollmentProperties.PAYMENT_ICON));
        } else if (PaymentCredentialEnrollmentProperties.CONTINUE_BUTTON_CALLBACK == propertyKey) {
            view.mContinueButton.setOnClickListener((v) -> {
                model.get(PaymentCredentialEnrollmentProperties.CONTINUE_BUTTON_CALLBACK).run();
            });
        } else if (PaymentCredentialEnrollmentProperties.PAYMENT_INSTRUMENT_LABEL == propertyKey) {
            view.mPaymentInstrumentLabel.setText(
                    model.get(PaymentCredentialEnrollmentProperties.PAYMENT_INSTRUMENT_LABEL));
        } else if (PaymentCredentialEnrollmentProperties.CANCEL_BUTTON_CALLBACK == propertyKey) {
            view.mCancelButton.setOnClickListener((v) -> {
                model.get(PaymentCredentialEnrollmentProperties.CANCEL_BUTTON_CALLBACK).run();
            });
        } else if (PaymentCredentialEnrollmentProperties.INCOGNITO_TEXT_VISIBLE == propertyKey) {
            boolean visible =
                    model.get(PaymentCredentialEnrollmentProperties.INCOGNITO_TEXT_VISIBLE);
            view.mIncognitoText.setVisibility(visible ? View.VISIBLE : View.GONE);
        }
    }
}
