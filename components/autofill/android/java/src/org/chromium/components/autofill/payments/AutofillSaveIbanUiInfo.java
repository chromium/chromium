// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill.payments;

import android.text.TextUtils;

import androidx.annotation.DrawableRes;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import java.util.Collections;
import java.util.List;

@JNINamespace("autofill")
/**
 * The android version of the C++ AutofillSaveIbanUiInfo providing UI resources for the save IBAN
 * bottom sheet.
 */
public class AutofillSaveIbanUiInfo {
    private final String mAcceptText;
    private final String mCancelText;
    // Description is empty for local save.
    private final String mDescriptionText;
    // The obfuscated value of IBAN being saved, e.g. CH **8009.
    private final String mIbanLabel;
    // This should be empty for local save.
    private final List<LegalMessageLine> mLegalMessageLines;
    // LogoIcon is 0 for local save.
    private final @DrawableRes int mLogoIcon;
    private final String mTitleText;

    public String getAcceptText() {
        return mAcceptText;
    }

    public String getCancelText() {
        return mCancelText;
    }

    public String getDescriptionText() {
        return mDescriptionText;
    }

    public String getIbanLabel() {
        return mIbanLabel;
    }

    public List<LegalMessageLine> getLegalMessageLines() {
        return mLegalMessageLines;
    }

    @DrawableRes
    public int getLogoIcon() {
        return mLogoIcon;
    }

    public String getTitleText() {
        return mTitleText;
    }

    @CalledByNative
    @VisibleForTesting
    /** Construct the delegate given all the members. */
    /* package */ AutofillSaveIbanUiInfo(
            @JniType("std::u16string") String acceptText,
            @JniType("std::u16string") String cancelText,
            @JniType("std::u16string") String descriptionText,
            @JniType("std::u16string") String ibanLabel,
            @Nullable List<LegalMessageLine> legalMessageLines,
            @DrawableRes int logoIcon,
            @JniType("std::u16string") String titleText) {
        mAcceptText = acceptText;
        mCancelText = cancelText;
        mDescriptionText = descriptionText;
        mIbanLabel = ibanLabel;
        if (legalMessageLines == null) {
            legalMessageLines = Collections.emptyList();
        }
        mLegalMessageLines = Collections.unmodifiableList(legalMessageLines);
        mLogoIcon = logoIcon;
        mTitleText = titleText;
    }

    /** Builder for {@link AutofillSaveIbanUiInfo} */
    @VisibleForTesting
    public static class Builder {
        private String mAcceptText;
        private String mCancelText;
        private String mDescriptionText;
        private String mIbanLabel;
        private List<LegalMessageLine> mLegalMessageLines;
        @DrawableRes private int mLogoIcon;
        private String mTitleText;

        public Builder withAcceptText(String acceptText) {
            mAcceptText = acceptText;
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

        public Builder withIbanLabel(String ibanLabel) {
            mIbanLabel = ibanLabel;
            return this;
        }

        public Builder withLegalMessageLines(List<LegalMessageLine> legalMessageLines) {
            mLegalMessageLines = Collections.unmodifiableList(legalMessageLines);
            return this;
        }

        public Builder withLogoIcon(@DrawableRes int logoIcon) {
            mLogoIcon = logoIcon;
            return this;
        }

        public Builder withTitleText(String titleText) {
            mTitleText = titleText;
            return this;
        }

        /** Create the {@link AutofillSaveIbanUiInfo} object. */
        public AutofillSaveIbanUiInfo build() {
            // The asserts are only checked in tests and in some Canary builds but not in
            // production. This is intended as we don't want to crash Chrome production for the
            // below checks.
            assert mIbanLabel != null && !TextUtils.isEmpty(mIbanLabel)
                    : "IBAN value cannot be null or empty.";
            return new AutofillSaveIbanUiInfo(
                    mAcceptText,
                    mCancelText,
                    mDescriptionText,
                    mIbanLabel,
                    mLegalMessageLines,
                    mLogoIcon,
                    mTitleText);
        }
    }
}
