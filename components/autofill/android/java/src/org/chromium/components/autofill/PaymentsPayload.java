// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.autofill.AutofillSuggestion.Payload;

import java.util.Objects;

@JNINamespace("autofill")
@NullMarked
public final class PaymentsPayload implements Payload {
    private final String mLabelContentDescription;
    private final boolean mShouldDisplayTermsAvailable;
    private final String mGuid;
    private final boolean mIsLocalPaymentsMethod;
    private @Nullable Long mExtractedAmount;

    /**
     * Constructs a payload object for the TouchToFillPaymentMethod bottom sheet.
     *
     * @param labelContentDescription Accessibility content description for the main text in the
     *     bottom sheet. This description is used by accessibility services like screen readers to
     *     interpret the content for users.
     * @param shouldDisplayTermsAvailable Whether the terms message is displayed.
     * @param guid The payment method identifier associated with the suggestion.
     * @param isLocalPaymentsMethod Whether the payments method associated with the suggestion is
     *     local.
     */
    @CalledByNative
    public PaymentsPayload(
            @JniType("std::u16string") String labelContentDescription,
            boolean shouldDisplayTermsAvailable,
            @JniType("std::string") String guid,
            boolean isLocalPaymentsMethod) {
        mLabelContentDescription = labelContentDescription;
        mShouldDisplayTermsAvailable = shouldDisplayTermsAvailable;
        mGuid = guid;
        mIsLocalPaymentsMethod = isLocalPaymentsMethod;
    }

    public String getLabelContentDescription() {
        return mLabelContentDescription;
    }

    public boolean shouldDisplayTermsAvailable() {
        return mShouldDisplayTermsAvailable;
    }

    public String getGuid() {
        return mGuid;
    }

    public boolean isLocalPaymentsMethod() {
        return mIsLocalPaymentsMethod;
    }

    public void setExtractedAmount(@Nullable Long extractedAmount) {
        mExtractedAmount = extractedAmount;
    }

    public @Nullable Long getExtractedAmount() {
        return mExtractedAmount;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) {
            return true;
        }
        if (!(o instanceof PaymentsPayload other)) {
            return false;
        }
        return this.mLabelContentDescription.equals(other.mLabelContentDescription)
                && this.mShouldDisplayTermsAvailable == other.mShouldDisplayTermsAvailable
                && this.mGuid.equals(other.mGuid)
                && this.mIsLocalPaymentsMethod == other.mIsLocalPaymentsMethod
                && Objects.equals(this.mExtractedAmount, other.mExtractedAmount);
    }

    @Override
    public int hashCode() {
        return Objects.hash(
                mLabelContentDescription,
                mShouldDisplayTermsAvailable,
                mGuid,
                mIsLocalPaymentsMethod,
                mExtractedAmount);
    }
}
