// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.sync;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;

/** Java version of the native LocalDataDescription struct. */
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

    @VisibleForTesting
    public int itemCount() {
        return mItemCount;
    }
}
