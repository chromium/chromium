// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.secure_payment_confirmation;

import android.content.Context;
import android.text.SpannableString;
import android.text.method.LinkMovementMethod;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.RelativeLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import org.chromium.components.payments.R;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;
import org.chromium.ui.widget.TextViewWithClickableSpans;

/**
 * The view of the SecurePaymentConfirmation No Matching Credentials UI. This view does not have a
 * peeked or half-open state. It has a fixed height, which is the height of the visible content
 * area. It informs the user that additional steps are needed to verify the payment.
 */
public class SecurePaymentConfirmationNoMatchingCredView {
    private final RelativeLayout mContentView;
    private final ScrollView mScrollView;

    /* package */ final ImageView mHeaderImage;
    /* package */ final TextView mDescription;
    /* package */ final Button mOkButton;
    /* package */ final TextViewWithClickableSpans mOptOutText;

    /**
     * @param context The Android Context used to inflate the View.
     * @param origin the origin of the merchant page.
     * @param rpId The relying party ID of the SPC credential.
     * @param showOptOut Whether to display the opt out UX to the user.
     * @param responseCallback Invoked when users respond to the UI.
     * @param optOutCallback Invoked if the user elects to opt out.
     */
    /* package */ SecurePaymentConfirmationNoMatchingCredView(
            Context context,
            String origin,
            String rpId,
            boolean showOptOut,
            Runnable responseCallback,
            Runnable optOutCallback) {
        mContentView =
                (RelativeLayout)
                        LayoutInflater.from(context)
                                .inflate(
                                        R.layout.secure_payment_confirmation_no_credential_match_ui,
                                        null);
        mScrollView = (ScrollView) mContentView.findViewById(R.id.scroll_view);

        mHeaderImage =
                (ImageView) mContentView.findViewById(R.id.secure_payment_confirmation_image);
        mDescription =
                (TextView)
                        mContentView.findViewById(
                                R.id.secure_payment_confirmation_nocredmatch_description);
        mOkButton = (Button) mContentView.findViewById(R.id.ok_button);
        mOptOutText =
                (TextViewWithClickableSpans)
                        mContentView.findViewById(
                                R.id.secure_payment_confirmation_nocredmatch_opt_out);
        mHeaderImage.setImageResource(R.drawable.save_card);

        String formattedDescription =
                String.format(
                        context.getResources()
                                .getString(R.string.no_matching_credential_description),
                        origin);
        mDescription.setText(formattedDescription);
        mOptOutText.setText(getOptOutText(context, rpId, optOutCallback));
        mOptOutText.setMovementMethod(LinkMovementMethod.getInstance());
        mOptOutText.setVisibility(showOptOut ? View.VISIBLE : View.GONE);
        mOkButton.setOnClickListener((v) -> responseCallback.run());
    }

    /* package */ View getContentView() {
        return mContentView;
    }

    /* package */ int getScrollY() {
        return mScrollView.getScrollY();
    }

    /**
     * Attempt to determine whether the current device is a tablet or not. This method is quite
     * inaccurate, but is only used for customizing the opt out UX and so getting it wrong is
     * low-cost.
     */
    private boolean isTablet(Context context) {
        return DeviceFormFactor.isNonMultiDisplayContextOnTablet(context);
    }

    private SpannableString getOptOutText(Context context, String rpId, Runnable optOutCallback) {
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
