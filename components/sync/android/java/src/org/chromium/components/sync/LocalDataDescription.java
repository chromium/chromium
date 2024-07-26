// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.sync;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

/** Java version of the native LocalDataDescription struct. */
@JNINamespace("syncer")
public class LocalDataDescription {
    private final int mItemCount;
    private final String[] mDomains;
    private final int mDomainCount;

    /** Constructs a LocalDataDescription with the provided parameters */
    @CalledByNative
    public LocalDataDescription(int itemCount, String[] domains, int domainCount) {
        mItemCount = itemCount;
        mDomains = domains;
        mDomainCount = domainCount;
    }

    /**
     * Returns a string that summarizes the domain content of the description, meant to be consumed
     * by the UI. Must not be called if itemCount() is 0.
     *
     * @return The display text.
     */
    public String getDomainsDisplayText() {
        assert mItemCount > 0;
        assert mDomains.length > 0;
        return LocalDataDescriptionJni.get()
                .getDomainsDisplayText(mItemCount, mDomains, mDomainCount);
    }

    @VisibleForTesting
    public int itemCount() {
        return mItemCount;
    }

    public int domainCount() {
        return mDomainCount;
    }

    @NativeMethods
    interface Natives {
        @JniType("std::u16string")
        String getDomainsDisplayText(
                int itemCount,
                @JniType("std::vector<std::string>") String[] domains,
                int domainCount);
    }
}
