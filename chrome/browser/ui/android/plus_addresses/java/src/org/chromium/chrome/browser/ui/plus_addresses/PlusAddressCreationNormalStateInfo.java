// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.plus_addresses;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.url.GURL;

import java.util.Objects;

/** Contains necessary information to show a meaningful error message to the user. */
@JNINamespace("plus_addresses")
class PlusAddressCreationNormalStateInfo {
    private final String mTitle;
    private final String mDescription;
    // The notice is empty in the case the user has already accepted the bottom sheet with the
    // onboarding message.
    private final String mNotice;
    private final String mProposedPlusAddressPlaceholder;
    private final String mConfirmText;
    // The cancel button is not shown if the notice message is empty. The cancel text is empty in
    // this case.
    private final String mCancelText;
    // TODO: crbug.com/354881207 - Remove once enhanced error handling is launched.
    private final String mErrorReportInstruction;
    private final GURL mLearnMoreUrl;
    private final GURL mErrorReportUrl;

    @VisibleForTesting
    @CalledByNative
    PlusAddressCreationNormalStateInfo(
            @JniType("std::u16string") String title,
            @JniType("std::u16string") String description,
            @JniType("std::u16string") String notice,
            @JniType("std::u16string") String proposedPlusAddressPlaceholder,
            @JniType("std::u16string") String confirmText,
            @JniType("std::u16string") String cancelText,
            @JniType("std::u16string") String errorReportInstruction,
            @JniType("GURL") GURL learnMoreUrl,
            @JniType("GURL") GURL errorReportUrl) {
        mTitle = Objects.requireNonNull(title, "Title can't be null");
        mDescription = Objects.requireNonNull(description, "Description can't be null");
        mNotice = Objects.requireNonNull(notice, "Notice can't be null");
        mProposedPlusAddressPlaceholder =
                Objects.requireNonNull(
                        proposedPlusAddressPlaceholder,
                        "Proposed plus address placeholder can't be null");
        mConfirmText = Objects.requireNonNull(confirmText, "Confirm button text can't be null");
        mCancelText = Objects.requireNonNull(cancelText, "Cancel button text can't be null");
        mErrorReportInstruction =
                Objects.requireNonNull(
                        errorReportInstruction, "Error report instruction can't be null");
        mLearnMoreUrl = Objects.requireNonNull(learnMoreUrl, "Learn more url can't be null");
        mErrorReportUrl = Objects.requireNonNull(errorReportUrl, "Error report url can't be null");
    }

    public String getTitle() {
        return mTitle;
    }

    public String getDescription() {
        return mDescription;
    }

    public String getNotice() {
        return mNotice;
    }

    public String getProposedPlusAddressPlaceholder() {
        return mProposedPlusAddressPlaceholder;
    }

    public String getConfirmText() {
        return mConfirmText;
    }

    public String getCancelText() {
        return mCancelText;
    }

    public String getErrorReportInstruction() {
        return mErrorReportInstruction;
    }

    public GURL getLearnMoreUrl() {
        return mLearnMoreUrl;
    }

    public GURL getErrorReportUrl() {
        return mErrorReportUrl;
    }
}
