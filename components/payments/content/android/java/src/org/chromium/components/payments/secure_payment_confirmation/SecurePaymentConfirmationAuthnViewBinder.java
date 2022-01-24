// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.secure_payment_confirmation;

import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * The view binder of the SecurePaymentConfirmation Authn UI, which is stateless. It is called to
 * bind a given model to a given view. Should contain as little business logic as possible.
 */
/* package */ class SecurePaymentConfirmationAuthnViewBinder {
    /* package */ static void bind(
            PropertyModel model, SecurePaymentConfirmationAuthnView view, PropertyKey propertyKey) {
        if (SecurePaymentConfirmationAuthnProperties.STORE_ORIGIN == propertyKey) {
            String origin = UrlFormatter.formatOriginForSecurityDisplay(
                    model.get(SecurePaymentConfirmationAuthnProperties.STORE_ORIGIN),
                    SchemeDisplay.OMIT_HTTP_AND_HTTPS);
            view.mStoreOrigin.setText(origin);
        } else if (SecurePaymentConfirmationAuthnProperties.PAYMENT_ICON == propertyKey) {
            view.mPaymentIcon.setImageDrawable(
                    model.get(SecurePaymentConfirmationAuthnProperties.PAYMENT_ICON));
        } else if (SecurePaymentConfirmationAuthnProperties.PAYMENT_INSTRUMENT_LABEL
                == propertyKey) {
            view.mPaymentInstrumentLabel.setText(
                    model.get(SecurePaymentConfirmationAuthnProperties.PAYMENT_INSTRUMENT_LABEL));
        } else if (SecurePaymentConfirmationAuthnProperties.TOTAL == propertyKey) {
            view.mTotal.setText(model.get(SecurePaymentConfirmationAuthnProperties.TOTAL));
        } else if (SecurePaymentConfirmationAuthnProperties.CURRENCY == propertyKey) {
            view.mCurrency.setText(model.get(SecurePaymentConfirmationAuthnProperties.CURRENCY));
        } else if (SecurePaymentConfirmationAuthnProperties.CONTINUE_BUTTON_CALLBACK
                == propertyKey) {
            view.mContinueButton.setOnClickListener((v) -> {
                model.get(SecurePaymentConfirmationAuthnProperties.CONTINUE_BUTTON_CALLBACK).run();
            });
        } else if (SecurePaymentConfirmationAuthnProperties.CANCEL_BUTTON_CALLBACK == propertyKey) {
            view.mCancelButton.setOnClickListener((v) -> {
                model.get(SecurePaymentConfirmationAuthnProperties.CANCEL_BUTTON_CALLBACK).run();
            });
        }
    }
}
