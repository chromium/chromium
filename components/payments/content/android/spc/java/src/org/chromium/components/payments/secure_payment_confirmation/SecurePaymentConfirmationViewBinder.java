// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.secure_payment_confirmation;

import android.graphics.drawable.Drawable;
import android.util.Pair;
import android.view.View;
import android.view.ViewGroup.LayoutParams;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * The view binder of the SecurePaymentConfirmation UI, which is stateless. It is called to bind a
 * given model to a given view. Should contain as little business logic as possible.
 */
@NullMarked
/* package */ class SecurePaymentConfirmationViewBinder {
    /* package */ static void bind(
            PropertyModel model, SecurePaymentConfirmationView view, PropertyKey propertyKey) {
        if (SecurePaymentConfirmationProperties.SHOWS_ISSUER_NETWORK_ICONS == propertyKey) {
            if (model.get(SecurePaymentConfirmationProperties.SHOWS_ISSUER_NETWORK_ICONS)) {
                view.mIssuerNetworkIconsRow.setVisibility(View.VISIBLE);
                view.mHeaderImage.setVisibility(View.GONE);
            } else {
                view.mHeaderImage.setVisibility(View.VISIBLE);
                view.mIssuerNetworkIconsRow.setVisibility(View.GONE);
            }
        } else if (SecurePaymentConfirmationProperties.ISSUER_ICON == propertyKey) {
            view.mIssuerIcon.setImageDrawable(
                    model.get(SecurePaymentConfirmationProperties.ISSUER_ICON));
        } else if (SecurePaymentConfirmationProperties.NETWORK_ICON == propertyKey) {
            view.mNetworkIcon.setImageDrawable(
                    model.get(SecurePaymentConfirmationProperties.NETWORK_ICON));
        } else if (SecurePaymentConfirmationProperties.TITLE == propertyKey) {
            view.mTitle.setText(model.get(SecurePaymentConfirmationProperties.TITLE));
        } else if (SecurePaymentConfirmationProperties.STORE_LABEL == propertyKey) {
            view.mStoreLabel.setText(model.get(SecurePaymentConfirmationProperties.STORE_LABEL));
        } else if (SecurePaymentConfirmationProperties.PAYMENT_ICON == propertyKey) {
            Pair<Drawable, Boolean> iconInfo =
                    model.get(SecurePaymentConfirmationProperties.PAYMENT_ICON);
            view.mPaymentIcon.setImageDrawable(iconInfo.first);
            // We normally override the input icon's dimensions, to stop developers from passing
            // arbitrary sized icons. However if we're using the default payment icon we should just
            // let it use its intrinsic sizing.
            if (iconInfo.second) {
                view.mPaymentIcon.getLayoutParams().height = LayoutParams.WRAP_CONTENT;
                view.mPaymentIcon.getLayoutParams().width = LayoutParams.WRAP_CONTENT;
            }
        } else if (SecurePaymentConfirmationProperties.PAYMENT_INSTRUMENT_LABEL == propertyKey) {
            view.mPaymentInstrumentLabel.setText(
                    model.get(SecurePaymentConfirmationProperties.PAYMENT_INSTRUMENT_LABEL));
        } else if (SecurePaymentConfirmationProperties.CURRENCY == propertyKey) {
            view.mCurrency.setText(model.get(SecurePaymentConfirmationProperties.CURRENCY));
        } else if (SecurePaymentConfirmationProperties.TOTAL == propertyKey) {
            view.mTotal.setText(model.get(SecurePaymentConfirmationProperties.TOTAL));
        } else if (SecurePaymentConfirmationProperties.OPT_OUT_TEXT == propertyKey) {
            if (model.get(SecurePaymentConfirmationProperties.OPT_OUT_TEXT) == null) {
                view.mOptOutText.setVisibility(View.GONE);
            } else {
                view.mOptOutText.setVisibility(View.VISIBLE);
                view.mOptOutText.setText(
                        model.get(SecurePaymentConfirmationProperties.OPT_OUT_TEXT));
            }
        } else if (SecurePaymentConfirmationProperties.FOOTNOTE == propertyKey) {
            if (model.get(SecurePaymentConfirmationProperties.FOOTNOTE) == null) {
                view.mFootnote.setVisibility(View.GONE);
            } else {
                view.mFootnote.setVisibility(View.VISIBLE);
                view.mFootnote.setText(model.get(SecurePaymentConfirmationProperties.FOOTNOTE));
            }
        } else if (SecurePaymentConfirmationProperties.CONTINUE_BUTTON_LABEL == propertyKey) {
            view.mContinueButton.setText(
                    model.get(SecurePaymentConfirmationProperties.CONTINUE_BUTTON_LABEL));
        }
    }
}
