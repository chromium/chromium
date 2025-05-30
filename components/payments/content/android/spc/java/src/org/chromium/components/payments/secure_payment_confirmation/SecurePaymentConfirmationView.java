// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.secure_payment_confirmation;

import android.content.Context;
import android.text.method.LinkMovementMethod;
import android.view.LayoutInflater;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.payments.R;
import org.chromium.ui.widget.TextViewWithClickableSpans;

/**
 * The view of the SecurePaymentConfirmation UI. This view does not have a peeked or half-open
 * state. It has a fixed height, which is the height of the visible content area. It shows the
 * payment details and provides the option to continue with the payment or to cancel.
 */
@NullMarked
/* package */ class SecurePaymentConfirmationView {
    /* package */ final RelativeLayout mContentView;
    /* package */ final ScrollView mScrollView;
    /* package */ final ImageView mHeaderImage;
    /* package */ final LinearLayout mIssuerNetworkIconsRow;
    /* package */ final ImageView mIssuerIcon;
    /* package */ final ImageView mNetworkIcon;
    /* package */ final TextView mTitle;
    /* package */ final TextView mStoreLabel;
    /* package */ final ImageView mPaymentIcon;
    /* package */ final TextView mPaymentInstrumentLabel;
    /* package */ final TextView mCurrency;
    /* package */ final TextView mTotal;
    /* package */ final TextViewWithClickableSpans mOptOutText;
    /* package */ final TextViewWithClickableSpans mFootnote;
    /* package */ final Button mContinueButton;
    /* package */ final Button mCancelButton;

    /* package */ SecurePaymentConfirmationView(Context context) {
        mContentView =
                (RelativeLayout)
                        LayoutInflater.from(context)
                                .inflate(R.layout.secure_payment_confirmation, null);
        mScrollView = mContentView.findViewById(R.id.scroll_view);
        mHeaderImage = mContentView.findViewById(R.id.secure_payment_confirmation_image);
        mIssuerNetworkIconsRow = mContentView.findViewById(R.id.issuer_network_icons_row);
        mIssuerIcon = mContentView.findViewById(R.id.issuer_icon);
        mNetworkIcon = mContentView.findViewById(R.id.network_icon);
        mTitle = mContentView.findViewById(R.id.secure_payment_confirmation_title);
        mStoreLabel = mContentView.findViewById(R.id.store);
        mPaymentIcon = mContentView.findViewById(R.id.payment_icon);
        mPaymentInstrumentLabel = mContentView.findViewById(R.id.payment);
        mCurrency = mContentView.findViewById(R.id.currency);
        mTotal = mContentView.findViewById(R.id.total);
        mOptOutText =
                mContentView.findViewById(R.id.secure_payment_confirmation_nocredmatch_opt_out);
        mFootnote = mContentView.findViewById(R.id.secure_payment_confirmation_footnote);
        mContinueButton = mContentView.findViewById(R.id.continue_button);
        mCancelButton = mContentView.findViewById(R.id.cancel_button);

        mHeaderImage.setImageResource(R.drawable.save_card);
        mOptOutText.setMovementMethod(LinkMovementMethod.getInstance());
        mFootnote.setMovementMethod(LinkMovementMethod.getInstance());
    }
}
