// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill.payments;

import androidx.annotation.DrawableRes;
import androidx.annotation.VisibleForTesting;

import com.google.common.collect.ImmutableList;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.util.List;

@JNINamespace("autofill")
/**
 * The android version of the C++ AutofillSaveCardUiInfo providing text and icons.
 *
 * Fields not needed by the save card bottom sheet UI on Android are not present.
 */
public class AutofillSaveCardUiInfo {
    private final boolean mIsForUpload;
    private final @DrawableRes int mLogoIcon;
    private final @DrawableRes int mIssuerIcon;
    private final ImmutableList<LegalMessageLine> mLegalMessageLines;
    private final String mCardLabel;
    private final String mCardSubLabel;
    private final String mCardDescription;
    private final String mTitleText;
    private final String mConfirmText;
    private final String mCancelText;
    private final String mDescriptionText;
    private final boolean mIsGooglePayBrandingEnabled;

    public boolean isForUpload() {
        return mIsForUpload;
    }

    @DrawableRes
    public int getLogoIcon() {
        return mLogoIcon;
    }

    /** @return an immutable list of legal message lines. */
    public ImmutableList<LegalMessageLine> getLegalMessageLines() {
        return ImmutableList.copyOf(mLegalMessageLines);
    }

    /** @return a CardDetail of the issuer icon, label, and sub label. */
    public CardDetail getCardDetail() {
        return new CardDetail(mIssuerIcon, mCardLabel, mCardSubLabel);
    }

    /** @return an accessibility description of the card. */
    public String getCardDescription() {
        return mCardDescription;
    }

    public String getTitleText() {
        return mTitleText;
    }

    public String getConfirmText() {
        return mConfirmText;
    }

    public String getCancelText() {
        return mCancelText;
    }

    public String getDescriptionText() {
        return mDescriptionText;
    }

    public boolean isGooglePayBrandingEnabled() {
        return mIsGooglePayBrandingEnabled;
    }

    // LINT.IfChange
    @CalledByNative
    @VisibleForTesting
    /** Construct the delegate given all the members. */
    /*package*/ AutofillSaveCardUiInfo(boolean isForUpload, @DrawableRes int logoIcon,
            @DrawableRes int issuerIcon, List<LegalMessageLine> legalMessageLines, String cardLabel,
            String cardSubLabel, String cardDescription, String titleText, String confirmText,
            String cancelText, boolean isGooglePayBrandingEnabled, String descriptionText) {
        mIsForUpload = isForUpload;
        mLogoIcon = logoIcon;
        mIssuerIcon = issuerIcon;
        if (legalMessageLines == null) {
            legalMessageLines = ImmutableList.of();
        }
        mLegalMessageLines = ImmutableList.copyOf(legalMessageLines);
        mCardLabel = cardLabel;
        mCardSubLabel = cardSubLabel;
        mCardDescription = cardDescription;
        mTitleText = titleText;
        mConfirmText = confirmText;
        mCancelText = cancelText;
        mIsGooglePayBrandingEnabled = isGooglePayBrandingEnabled;
        mDescriptionText = descriptionText;
    }
    // LINT.ThenChange(//chrome/browser/ui/android/autofill/autofill_save_card_bottom_sheet_bridge.cc)

    /** Builder for {@link AutofillSaveCardUiInfo} */
    @VisibleForTesting
    public static class Builder {
        private boolean mIsForUpload;
        @DrawableRes
        private int mLogoIcon;
        private CardDetail mCardDetail;
        private String mCardDescription;
        private ImmutableList<LegalMessageLine> mLegalMessageLines = ImmutableList.of();
        private String mTitleText;
        private String mConfirmText;
        private String mCancelText;
        private boolean mIsGooglePayBrandingEnabled;
        private String mDescriptionText;

        public Builder withIsForUpload(boolean isForUpload) {
            mIsForUpload = isForUpload;
            return this;
        }

        public Builder withLogoIcon(@DrawableRes int logoIcon) {
            mLogoIcon = logoIcon;
            return this;
        }

        public Builder withCardDetail(CardDetail cardDetail) {
            mCardDetail = cardDetail;
            return this;
        }

        public Builder withCardDescription(String cardDescription) {
            mCardDescription = cardDescription;
            return this;
        }

        public Builder withLegalMessageLines(List<LegalMessageLine> legalMessageLines) {
            mLegalMessageLines = ImmutableList.copyOf(legalMessageLines);
            return this;
        }

        public Builder withTitleText(String titleText) {
            mTitleText = titleText;
            return this;
        }

        public Builder withConfirmText(String confirmText) {
            mConfirmText = confirmText;
            return this;
        }

        public Builder withCancelText(String cancelText) {
            mCancelText = cancelText;
            return this;
        }

        public Builder withIsGooglePayBrandingEnabled(boolean isGooglePayBrandingEnabled) {
            mIsGooglePayBrandingEnabled = isGooglePayBrandingEnabled;
            return this;
        }

        public Builder withDescriptionText(String descriptionText) {
            mDescriptionText = descriptionText;
            return this;
        }

        /**
         * Create the {@link AutofillSaveCardUiInfo} object.
         */
        public AutofillSaveCardUiInfo build() {
            return new AutofillSaveCardUiInfo(mIsForUpload, mLogoIcon,
                    mCardDetail.issuerIconDrawableId, mLegalMessageLines, mCardDetail.label,
                    mCardDetail.subLabel, mCardDescription, mTitleText, mConfirmText, mCancelText,
                    mIsGooglePayBrandingEnabled, mDescriptionText);
        }
    }
}
