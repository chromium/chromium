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

    /**
     * @param context The Android Context used to inflate the View.
     * @param origin the origin of the merchant page.
     * @param callback Invoked when users respond to the UI.
     */
    /* package */ SecurePaymentConfirmationNoMatchingCredView(
            Context context, String origin, Runnable callback) {
        mContentView = (RelativeLayout) LayoutInflater.from(context).inflate(
                R.layout.secure_payment_confirmation_no_credential_match_ui, null);
        mScrollView = (ScrollView) mContentView.findViewById(R.id.scroll_view);

        mHeaderImage =
                (ImageView) mContentView.findViewById(R.id.secure_payment_confirmation_image);
        mDescription = (TextView) mContentView.findViewById(
                R.id.secure_payment_confirmation_nocredmatch_description);
        mOkButton = (Button) mContentView.findViewById(R.id.ok_button);
        mHeaderImage.setImageResource(R.drawable.save_card);

        String formattedDescription = String.format(
                context.getResources().getString(R.string.no_matching_credential_description),
                origin);
        mDescription.setText(formattedDescription);
        mOkButton.setOnClickListener((v) -> callback.run());
    }

    /* package */ View getContentView() {
        return mContentView;
    }

    /* package */ int getScrollY() {
        return mScrollView.getScrollY();
    }
}
