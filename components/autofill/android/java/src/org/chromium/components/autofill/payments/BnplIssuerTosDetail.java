// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill.payments;

import android.text.SpannableString;

import androidx.annotation.DrawableRes;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;

import java.util.List;
import java.util.Objects;
import java.util.function.Consumer;

/** Detailed BNPL issuer ToS to show in the BNPL issuer ToS bottonsheet. */
@NullMarked
public class BnplIssuerTosDetail {
    /** Legal messages. */
    public static class LegalMessages {
        /** Legal message lines. */
        public final List<LegalMessageLine> mLines;

        /** Function for opening links for the legal messages. */
        public final Consumer<String> mLinkOpener;

        /**
         * Constructs legal messages.
         *
         * @param lines The legal message lines. Must not be {@code null}.
         * @param linkOpener The link for the legal message. Must not be {@code null}.
         */
        public LegalMessages(List<LegalMessageLine> lines, Consumer<String> linkOpener) {
            mLines = Objects.requireNonNull(lines, "List of legal message lines can't be null");
            mLinkOpener = Objects.requireNonNull(linkOpener, "Link consumer can't be null");
        }
    }

    /** Icon id for the screen title. */
    private final @DrawableRes int mHeaderIconDrawableId;

    /** Drak theme icon id for the screen title. */
    private final @DrawableRes int mHeaderIconDarkDrawableId;

    /** Title text for the BNPL ToS screen. */
    private final String mTitle;

    /** Sign-in/create account message. */
    private final String mReviewText;

    /** Eligibility check message. */
    private final String mApproveText;

    /** Account link/unlink message. */
    private final SpannableString mLinkText;

    /** List of legal messages and function for opening url. */
    private final LegalMessages mLegalMessages;

    /**
     * Creates a new instance of the detailed card information.
     *
     * @param headerIconDrawableId Icon id for the screen title.
     * @param headerIconDarkDrawableId Drak theme icon id for the screen title.
     * @param title Title text for the BNPL ToS screen.
     * @param reviewText String for sign-in/create account message.
     * @param approveText String for eligibility check message.
     * @param linkText String for account link/unlink message.
     * @param legalMessages List of legal messages and function for opening url.
     */
    @CalledByNative
    public BnplIssuerTosDetail(
            @DrawableRes int headerIconDrawableId,
            @DrawableRes int headerIconDarkDrawableId,
            String title,
            String reviewText,
            String approveText,
            SpannableString linkText,
            LegalMessages legalMessages) {
        mHeaderIconDrawableId = headerIconDrawableId;
        mHeaderIconDarkDrawableId = headerIconDarkDrawableId;
        mTitle = title;
        mReviewText = reviewText;
        mApproveText = approveText;
        mLinkText = linkText;
        mLegalMessages = legalMessages;
    }

    public @DrawableRes int getHeaderIconDrawableId() {
        return mHeaderIconDrawableId;
    }

    public @DrawableRes int getHeaderIconDarkDrawableId() {
        return mHeaderIconDarkDrawableId;
    }

    public String getTitle() {
        return mTitle;
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

    public LegalMessages getLegalMessages() {
        return mLegalMessages;
    }
}
