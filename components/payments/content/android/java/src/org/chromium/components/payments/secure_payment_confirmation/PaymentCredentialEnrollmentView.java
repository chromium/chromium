// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.components.payments.secure_payment_confirmation;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.RelativeLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import org.chromium.components.payments.R;

/**
 * The view of the SecurePaymentConfirmation Enrollment UI. This view does not have a peeked or
 * half-open state. It has a fixed height, which is the height of the visible content area. It shows
 * the enrollment details and provides the option to continue with the enrollment or to cancel.
 */
/* package */ class PaymentCredentialEnrollmentView {
    private final RelativeLayout mContentView;
    private final ScrollView mScrollView;
    /* package */ final ImageView mHeaderImage;
    /* package */ final TextView mPaymentInstrumentLabel;
    /* package */ final TextView mIncognitoText;
    /* package */ final ImageView mPaymentIcon;
    /* package */ final Button mContinueButton;
    /* package */ final Button mCancelButton;

    /* package */ PaymentCredentialEnrollmentView(Context context) {
        mContentView = (RelativeLayout) LayoutInflater.from(context).inflate(
                R.layout.secure_payment_confirmation_enrollment_ui, null);
        mScrollView = (ScrollView) mContentView.findViewById(R.id.scroll_view);
        mHeaderImage =
                (ImageView) mContentView.findViewById(R.id.secure_payment_confirmation_image);
        mPaymentInstrumentLabel = (TextView) mContentView.findViewById(R.id.payment);
        mIncognitoText = (TextView) mContentView.findViewById(R.id.incognito_text);
        mPaymentIcon = (ImageView) mContentView.findViewById(R.id.payment_icon);
        mContinueButton = (Button) mContentView.findViewById(R.id.continue_button);
        mCancelButton = (Button) mContentView.findViewById(R.id.cancel_button);
        mHeaderImage.setImageResource(R.drawable.save_card);
    }

    /* package */ View getContentView() {
        return mContentView;
    }

    /* package */ int getScrollY() {
        return mScrollView.getScrollY();
    }
}
