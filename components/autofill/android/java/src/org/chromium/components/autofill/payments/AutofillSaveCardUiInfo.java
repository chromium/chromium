// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill.payments;

import androidx.annotation.DrawableRes;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import java.util.Collections;
import java.util.List;
import java.util.Objects;

/**
 * The android version of the C++ AutofillSaveCardUiInfo providing text and icons.
 *
 * <p>Fields not needed by the save card bottom sheet UI on Android are not present.
 */
@JNINamespace("autofill")
public class AutofillSaveCardUiInfo {
    private final boolean mIsForUpload;
    private final @DrawableRes int mLogoIcon;
    private final @DrawableRes int mIssuerIcon;
    private final List<LegalMessageLine> mLegalMessageLines;
    private final String mCardLabel;
    private final String mCardSubLabel;
    private final String mCardDescription;
    private final String mTitleText;
    private final String mConfirmText;
    private final String mCancelText;
    private final String mDescriptionText;
    private final String mLoadingDescription;
    private final boolean mIsGooglePayBrandingEnabled;

    public boolean isForUpload() {
        return mIsForUpload;
    }

    @DrawableRes
    public int getLogoIcon() {
        return mLogoIcon;
    }

    /**
     * @return an immutable list of legal message lines.
     */
    public List<LegalMessageLine> getLegalMessageLines() {
        return mLegalMessageLines;
    }

    /**
     * @return a CardDetail of the issuer icon, label, and sub label.
     */
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

    public String getLoadingDescription() {
        return mLoadingDescription;
    }

    public boolean isGooglePayBrandingEnabled() {
        return mIsGooglePayBrandingEnabled;
    }

    // LINT.IfChange
    /**
     * Construct the {@link AutofillSaveCardUiInfo} given all the members. This constructor is used
     * for native binding purposes.
     *
     * @param isForUpload {@code true} is the card is going to be saved on the server, {@code false}
     *     otherwise.
     * @param logoIcon The icon id used displayed at the top of the bottom sheet. This value is
     *     {@code 0} for local credit card save.
     * @param issuerIcon Credit card icon shown in the bottom sheet. This value is {@code 0} for
     *     local credit card save.
     * @param legalMessageLines A list of legal message strings with user help links. This list is
     *     empty for local save. Must not be {@code null}.
     * @param cardLabel A string that contains an obfuscated credit card number. Must not be {@code
     *     null}.
     * @param cardSubLabel A string that contains the credit card expiration date. Must not be
     *     {@code null}.
     * @param cardDescription A credit card description string used for accessibility. Must not be
     *     {@code null}.
     * @param titleText The title of the bottom sheet. Must not be {@code null}.
     * @param confirmText The UI string displayed on the confirm button. Must not be {@code null}.
     * @param cancelText The UI string displayed on the cancel button. Must not be {@code null}.
     * @param descriptionText The bottom sheet description UI string. Must not be {@code null}.
     * @param loadingDescription An accessibility strings for the loading view. Must not be {@code
     *     null}.
     * @param isGooglePayBrandingEnabled Whether Google Chrome branding is enabled for the build.
     */
    @CalledByNative
    @VisibleForTesting
    /*package*/ AutofillSaveCardUiInfo(
            boolean isForUpload,
            @DrawableRes int logoIcon,
            @DrawableRes int issuerIcon,
            @JniType("std::vector") List<LegalMessageLine> legalMessageLines,
            String cardLabel,
            String cardSubLabel,
            String cardDescription,
            String titleText,
            String confirmText,
            String cancelText,
            String descriptionText,
            String loadingDescription,
            boolean isGooglePayBrandingEnabled) {
        mIsForUpload = isForUpload;
        mLogoIcon = logoIcon;
        mIssuerIcon = issuerIcon;
        mLegalMessageLines =
                Collections.unmodifiableList(
                        Objects.requireNonNull(
                                legalMessageLines, "List of legal message lines can't be null"));
        mCardLabel = Objects.requireNonNull(cardLabel, "Card label can't be null");
        mCardSubLabel = Objects.requireNonNull(cardSubLabel, "Card sublabel can't be null");
        mCardDescription =
                Objects.requireNonNull(cardDescription, "Card description can't be null");
        mTitleText = Objects.requireNonNull(titleText, "Title text can't be null");
        mConfirmText = Objects.requireNonNull(confirmText, "Confirm text can't be null");
        mCancelText = Objects.requireNonNull(cancelText, "Cancel text can't be null");
        mDescriptionText =
                Objects.requireNonNull(descriptionText, "Description text can't be null");
        mLoadingDescription =
                Objects.requireNonNull(loadingDescription, "Loading description can't be null");
        mIsGooglePayBrandingEnabled = isGooglePayBrandingEnabled;
    }

    // LINT.ThenChange(//chrome/browser/ui/android/autofill/autofill_save_card_bottom_sheet_bridge.cc)

    /** Builder for {@link AutofillSaveCardUiInfo} */
    @VisibleForTesting
    public static class Builder {
        private boolean mIsForUpload;
        @DrawableRes private int mLogoIcon;
        private CardDetail mCardDetail;
        private String mCardDescription;
        private List<LegalMessageLine> mLegalMessageLines;
        private String mTitleText;
        private String mConfirmText;
        private String mCancelText;
        private String mDescriptionText;
        private String mLoadingDescription;
        private boolean mIsGooglePayBrandingEnabled;

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
            mLegalMessageLines = legalMessageLines;
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

        public Builder withDescriptionText(String descriptionText) {
            mDescriptionText = descriptionText;
            return this;
        }

        public Builder withLoadingDescription(String loadingDescription) {
            mLoadingDescription = loadingDescription;
            return this;
        }

        public Builder withIsGooglePayBrandingEnabled(boolean isGooglePayBrandingEnabled) {
            mIsGooglePayBrandingEnabled = isGooglePayBrandingEnabled;
            return this;
        }

        /** Create the {@link AutofillSaveCardUiInfo} object. */
        public AutofillSaveCardUiInfo build() {
            return new AutofillSaveCardUiInfo(
                    mIsForUpload,
                    mLogoIcon,
                    mCardDetail.issuerIconDrawableId,
                    mLegalMessageLines,
                    mCardDetail.label,
                    mCardDetail.subLabel,
                    mCardDescription,
                    mTitleText,
                    mConfirmText,
                    mCancelText,
                    mDescriptionText,
                    mLoadingDescription,
                    mIsGooglePayBrandingEnabled);
        }
    }
}
