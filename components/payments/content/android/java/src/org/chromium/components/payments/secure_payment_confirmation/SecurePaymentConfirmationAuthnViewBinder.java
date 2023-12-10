// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.secure_payment_confirmation;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.text.SpannableString;
import android.util.Pair;
import android.view.View;
import android.view.ViewGroup.LayoutParams;

import org.chromium.components.payments.R;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;

/**
 * The view binder of the SecurePaymentConfirmation Authn UI, which is stateless. It is called to
 * bind a given model to a given view. Should contain as little business logic as possible.
 */
/* package */ class SecurePaymentConfirmationAuthnViewBinder {
    /* package */ static void bind(
            PropertyModel model, SecurePaymentConfirmationAuthnView view, PropertyKey propertyKey) {
        if (SecurePaymentConfirmationAuthnProperties.STORE_LABEL == propertyKey) {
            view.mStoreLabel.setText(
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
        } else if (SecurePaymentConfirmationAuthnProperties.OPT_OUT_INFO == propertyKey) {
            SecurePaymentConfirmationAuthnView.OptOutInfo info =
                    model.get(SecurePaymentConfirmationAuthnProperties.OPT_OUT_INFO);
            view.mOptOutText.setVisibility(info.mShowOptOut ? View.VISIBLE : View.GONE);
            view.mOptOutText.setText(
                    getOptOutText(view.mContext, info.mRpId, info.mOptOutCallback));
        } else if (SecurePaymentConfirmationAuthnProperties.CONTINUE_BUTTON_CALLBACK
                == propertyKey) {
            view.mContinueButton.setOnClickListener(
                    (v) -> {
                        model.get(SecurePaymentConfirmationAuthnProperties.CONTINUE_BUTTON_CALLBACK)
                                .run();
                    });
        } else if (SecurePaymentConfirmationAuthnProperties.CANCEL_BUTTON_CALLBACK == propertyKey) {
            view.mCancelButton.setOnClickListener(
                    (v) -> {
                        model.get(SecurePaymentConfirmationAuthnProperties.CANCEL_BUTTON_CALLBACK)
                                .run();
                    });
        }
    }

    /**
     * Attempt to determine whether the current device is a tablet or not. This method is quite
     * inaccurate, but is only used for customizing the opt out UX and so getting it wrong is
     * low-cost.
     */
    private static boolean isTablet(Context context) {
        return DeviceFormFactor.isNonMultiDisplayContextOnTablet(context);
    }

    private static SpannableString getOptOutText(
            Context context, String rpId, Runnable optOutCallback) {
        String deviceString =
                context.getResources()
                        .getString(
                                isTablet(context)
                                        ? R.string.secure_payment_confirmation_this_tablet_label
                                        : R.string.secure_payment_confirmation_this_phone_label);
        String optOut =
                context.getResources()
                        .getString(
                                R.string.secure_payment_confirmation_opt_out_label,
                                deviceString,
                                rpId);
        NoUnderlineClickableSpan requestToDeleteSpan =
                new NoUnderlineClickableSpan(context, (widget) -> optOutCallback.run());
        return SpanApplier.applySpans(
                optOut, new SpanInfo("BEGIN_LINK", "END_LINK", requestToDeleteSpan));
    }
}
