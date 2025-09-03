// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid.data;

import android.graphics.Bitmap;

import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;

/** Holds data about a relying party which is displayed on the FedCM bottom sheet. */
@NullMarked
public class RelyingPartyData {
    private final String mRpForDisplay;
    private final String mIframeForDisplay;
    @Nullable private final Bitmap mRpIcon;

    /**
     * @param rpForDisplay The relying party eTLD+1 to be displayed.
     * @param iframeForDisplay The iframe eTLD+1 to be displayed.
     * @param rpIcon The icon of the relying party.
     */
    @CalledByNative
    public RelyingPartyData(
            @JniType("std::u16string") String rpForDisplay,
            @JniType("std::u16string") String iframeForDisplay,
            @Nullable Bitmap rpIcon) {
        mRpForDisplay = rpForDisplay;
        mIframeForDisplay = iframeForDisplay;
        mRpIcon = rpIcon;
    }

    /** Returns the relying party eTLD+1 to be displayed. */
    public String getRpForDisplay() {
        return mRpForDisplay;
    }

    /** Returns the iframe eTLD+1 to be displayed. */
    public String getIframeForDisplay() {
        return mIframeForDisplay;
    }

    /** Returns the icon of the relying party. */
    @Nullable
    public Bitmap getRpIcon() {
        return mRpIcon;
    }
}
