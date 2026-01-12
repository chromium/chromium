// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill.payments;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.build.annotations.NullMarked;

@JNINamespace("autofill")
@NullMarked
public class TouchToFillDisplayOptions {

    /** Indicates whether the "Scan credit card" option should be displayed in the TTF footer. */
    private boolean mShowScanCreditCard;

    /** Indicates whether the Google Pay logo should be displayed in the TTF header. */
    private boolean mShowGPayLogo;

    public TouchToFillDisplayOptions() {}

    public TouchToFillDisplayOptions showScanCreditCard(boolean show) {
        mShowScanCreditCard = show;
        return this;
    }

    public TouchToFillDisplayOptions showGPayLogo(boolean show) {
        mShowGPayLogo = show;
        return this;
    }

    /**
     * Static factory method used by the JNI bridge to create an instance from C++.
     *
     * @param showScanCreditCard Whether the "Scan Credit Card" action should be visible.
     * @param showGPayLogo Whether the Google Pay logo should be displayed in the footer.
     * @return A new instance of {@link TouchToFillDisplayOptions}.
     */
    @CalledByNative
    public static TouchToFillDisplayOptions create(
            boolean showScanCreditCard, boolean showGPayLogo) {
        return new TouchToFillDisplayOptions()
                .showScanCreditCard(showScanCreditCard)
                .showGPayLogo(showGPayLogo);
    }

    /**
     * @return True if the "Scan credit card" button should be visible.
     */
    public boolean shouldShowScanCreditCard() {
        return mShowScanCreditCard;
    }

    /**
     * @return True if the GPay logo should be visible.
     */
    public boolean shouldShowGPayLogo() {
        return mShowGPayLogo;
    }
}
