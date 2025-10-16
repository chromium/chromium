// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill.payments;

import androidx.annotation.DrawableRes;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;

/** Autofill BNPL issuer context information. */
@JNINamespace("autofill")
@NullMarked
public class BnplIssuerContext {
    private final @DrawableRes int mIconId;
    private final String mDisplayName;
    private final String mSelectionText;
    private final boolean mIsLinked;
    private final boolean mIsEligible;

    /**
     * Constructs a new BnplIssuerContext.
     *
     * @param iconId The resource ID of the issuer's icon.
     * @param displayName The name of the issuer to be displayed.
     * @param selectionText The selection text of the issuer to be displayed.
     * @param isLinked Whether the issuer is linked or not.
     * @param isEligible Whether the issuer is eligible to be selected.
     */
    @CalledByNative("BnplIssuerContext")
    public BnplIssuerContext(
            @DrawableRes int iconId,
            @JniType("std::u16string") String displayName,
            @JniType("std::u16string") String selectionText,
            boolean isLinked,
            boolean isEligible) {
        mIconId = iconId;
        mDisplayName = displayName;
        mSelectionText = selectionText;
        mIsLinked = isLinked;
        mIsEligible = isEligible;
    }

    /** Returns the resource ID of the issuer's icon. */
    public @DrawableRes int getIconId() {
        return mIconId;
    }

    /** Returns the name of the issuer to be displayed. */
    public String getDisplayName() {
        return mDisplayName;
    }

    /** Returns the selection text of the issuer to be displayed. */
    public String getSelectionText() {
        return mSelectionText;
    }

    /** Returns {@code true} if the issuer is linked. */
    public boolean isLinked() {
        return mIsLinked;
    }

    /** Returns {@code true} if the issuer is eligible to be selected. */
    public boolean isEligible() {
        return mIsEligible;
    }
}
