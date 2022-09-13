// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.secure_payment_confirmation;

import android.graphics.drawable.Drawable;
import android.util.Pair;
import android.view.ViewGroup.LayoutParams;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * The view binder of the SecurePaymentConfirmation Authn UI, which is stateless. It is called to
 * bind a given model to a given view. Should contain as little business logic as possible.
 */
/* package */ class SecurePaymentConfirmationAuthnViewBinder {
    /* package */ static void bind(
            PropertyModel model, SecurePaymentConfirmationAuthnView view, PropertyKey propertyKey) {
        if (SecurePaymentConfirmationAuthnProperties.STORE_LABEL == propertyKey) {
            view.mStoreOrigin.setText(
                    model.get(SecurePaymentConfirmationAuthnProperties.STORE_LABEL));
        } else if (SecurePaymentConfirmationAuthnProperties.PAYMENT_ICON == propertyKey) {
            Pair<Drawable, Boolean> iconInfo =
                    model.get(SecurePaymentConfirmationAuthnProperties.PAYMENT_ICON);
            view.mPaymentIcon.setImageDrawable(iconInfo.first);
            // We normally override the input icon's dimensions, to stop developers from passing
            // arbitrary sized icons. However if we're using the default payment icon we should just
            // let it use its intrinsic sizing.
            if (iconInfo.second) {
                view.mPaymentIcon.getLayoutParams().height = LayoutParams.WRAP_CONTENT;
                view.mPaymentIcon.getLayoutParams().width = LayoutParams.WRAP_CONTENT;
            }
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
