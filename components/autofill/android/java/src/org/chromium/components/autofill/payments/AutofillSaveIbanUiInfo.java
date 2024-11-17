// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill.payments;

import android.text.TextUtils;

import androidx.annotation.DrawableRes;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import java.util.Collections;
import java.util.List;
import java.util.Objects;

/**
 * The android version of the C++ AutofillSaveIbanUiInfo providing UI resources for the save IBAN
 * bottom sheet.
 */
@JNINamespace("autofill")
public class AutofillSaveIbanUiInfo {
    private final String mAcceptText;
    private final String mCancelText;
    // Description is empty for local save.
    private final String mDescriptionText;
    // The value of IBAN being saved, e.g. CH56 0483 5012 3456 7800 9.
    private final String mIbanValue;
    private final boolean mIsServerSave;
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

    public String getIbanValue() {
        return mIbanValue;
    }

    public boolean isServerSave() {
        return mIsServerSave;
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

    /**
     * Construct the {@link AutofillSaveIbanUiInfo} given all the members. This constructor is used
     * for native binding purposes.
     *
     * @param acceptText A UI string displayed on the confirm button. This value must not be {@code
     *     null}.
     * @param cancelText A UI string displayed on the cancel button. This value must not be {@code
     *     null}.
     * @param descriptionText A bottom sheet description UI string displayed below the bottom sheet
     *     title. This value must not be {@code null}.
     * @param ibanValue A string containing IBAN value. This value must not be {@code null}.
     * @param legalMessageLines A list of legal message strings with user help links. This list is
     *     empty for local save. Must not be {@code null}.
     * @param logoIcon A logo icon displayed at the top of the bottom sheet. This value is equals to
     *     {@code 0} for local save.
     * @param titleText A bottom sheet title UI string. This value must not be {@code null}.
     */
    @CalledByNative
    @VisibleForTesting
    /* package */ AutofillSaveIbanUiInfo(
            @JniType("std::u16string") String acceptText,
            @JniType("std::u16string") String cancelText,
            @JniType("std::u16string") String descriptionText,
            @JniType("std::u16string") String ibanValue,
            boolean isServerSave,
            @JniType("std::vector") List<LegalMessageLine> legalMessageLines,
            @DrawableRes int logoIcon,
            @JniType("std::u16string") String titleText) {
        mAcceptText = Objects.requireNonNull(acceptText, "Accept text can't be null");
        mCancelText = Objects.requireNonNull(cancelText, "Cancel text can't be null");
        mDescriptionText =
                Objects.requireNonNull(descriptionText, "Description text can't be null");
        mIbanValue = Objects.requireNonNull(ibanValue, "Iban value can't be null");
        mIsServerSave = isServerSave;
        mLegalMessageLines =
                Collections.unmodifiableList(
                        Objects.requireNonNull(
                                legalMessageLines, "List of legal message lines can't be null"));
        mLogoIcon = logoIcon;
        mTitleText = Objects.requireNonNull(titleText, "Title text can't be null");
    }

    /** Builder for {@link AutofillSaveIbanUiInfo} */
    @VisibleForTesting
    public static class Builder {
        private String mAcceptText;
        private String mCancelText;
        private String mDescriptionText;
        private String mIbanValue;
        private boolean mIsServerSave;
        private List<LegalMessageLine> mLegalMessageLines = Collections.EMPTY_LIST;
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

        public Builder withIbanValue(String ibanValue) {
            mIbanValue = ibanValue;
            return this;
        }

        public Builder withIsServerSave(boolean isServerSave) {
            mIsServerSave = isServerSave;
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
            assert mIbanValue != null && !TextUtils.isEmpty(mIbanValue)
                    : "IBAN value cannot be null or empty.";
            return new AutofillSaveIbanUiInfo(
                    mAcceptText,
                    mCancelText,
                    mDescriptionText,
                    mIbanValue,
                    mIsServerSave,
                    mLegalMessageLines,
                    mLogoIcon,
                    mTitleText);
        }
    }
}
