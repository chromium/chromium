// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill.payments;

import android.text.SpannableString;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;

/** Detailed BNPL issuer ToS to show in the BNPL issuer ToS bottonsheet. */
@NullMarked
public class BnplIssuerTosDetail {
    /** Sign-in/create account message. */
    private final String mReviewText;

    /** Eligibility check message. */
    private final String mApproveText;

    /** Account link/unlink message. */
    private final SpannableString mLinkText;

    /**
     * Creates a new instance of the detailed card information.
     *
     * @param reviewText String for sign-in/create account message.
     * @param approveText String for eligibility check message.
     * @param linkText String for account link/unlink message.
     */
    @CalledByNative
    public BnplIssuerTosDetail(String reviewText, String approveText, SpannableString linkText) {
        mReviewText = reviewText;
        mApproveText = approveText;
        mLinkText = linkText;
    }

    public String getReviewText() {
        return mReviewText;
    }

    public String getApproveText() {
        return mApproveText;
    }

    public SpannableString getLinkText() {
        return mLinkText;
    }
}
