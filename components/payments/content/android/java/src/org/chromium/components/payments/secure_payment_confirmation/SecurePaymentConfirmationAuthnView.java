// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.secure_payment_confirmation;

import android.content.Context;
import android.text.method.LinkMovementMethod;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.RelativeLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import org.chromium.components.payments.R;
import org.chromium.ui.widget.TextViewWithClickableSpans;

/**
 * The view of the SecurePaymentConfirmation Authn UI. This view does not have a peeked or half-open
 * state. It has a fixed height, which is the height of the visible content area. It shows the
 * payment details and provides the option to continue with the payment or to cancel.
 */
/* package */ class SecurePaymentConfirmationAuthnView {
    /**
     * Bundles the information necessary to show the opt out UX, if requested by the caller.
     * Bundling is necessary as the UI component is a single logical unit from the binder's
     * perspective.
     */
    /* package */ static class OptOutInfo {
        /* package */ final boolean mShowOptOut;
        /* package */ final String mRpId;
        /* package */ final Runnable mOptOutCallback;

        /* package */ OptOutInfo(boolean showOptOut, String rpId, Runnable optOutCallback) {
            mShowOptOut = showOptOut;
            mRpId = rpId;
            mOptOutCallback = optOutCallback;
        }
    }

    private final RelativeLayout mContentView;
    private final ScrollView mScrollView;

    /* package */ final Context mContext;
    /* package */ final ImageView mHeaderImage;
    /* package */ final TextView mStoreLabel;
    /* package */ final TextView mPaymentInstrumentLabel;
    /* package */ final ImageView mPaymentIcon;
    /* package */ final TextView mTotal;
    /* package */ final TextView mCurrency;
    /* package */ final Button mContinueButton;
    /* package */ final Button mCancelButton;
    /* package */ final TextViewWithClickableSpans mOptOutText;

    /* package */ SecurePaymentConfirmationAuthnView(Context context) {
        mContentView =
                (RelativeLayout)
                        LayoutInflater.from(context)
                                .inflate(R.layout.secure_payment_confirmation_authn_ui, null);
        mScrollView = (ScrollView) mContentView.findViewById(R.id.scroll_view);
        mContext = context;
        mHeaderImage =
                (ImageView) mContentView.findViewById(R.id.secure_payment_confirmation_image);
        mStoreLabel = (TextView) mContentView.findViewById(R.id.store);
        mPaymentInstrumentLabel = (TextView) mContentView.findViewById(R.id.payment);
        mPaymentIcon = (ImageView) mContentView.findViewById(R.id.payment_icon);
        mTotal = (TextView) mContentView.findViewById(R.id.total);
        mCurrency = (TextView) mContentView.findViewById(R.id.currency);
        mContinueButton = (Button) mContentView.findViewById(R.id.continue_button);
        mCancelButton = (Button) mContentView.findViewById(R.id.cancel_button);
        mOptOutText =
                (TextViewWithClickableSpans)
                        mContentView.findViewById(
                                R.id.secure_payment_confirmation_nocredmatch_opt_out);

        mHeaderImage.setImageResource(R.drawable.save_card);
        mOptOutText.setMovementMethod(LinkMovementMethod.getInstance());
    }

    /* package */ View getContentView() {
        return mContentView;
    }

    /* package */ int getScrollY() {
        return mScrollView.getScrollY();
    }
}
