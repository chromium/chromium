// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import android.os.Build;

import androidx.annotation.VisibleForTesting;

@VisibleForTesting
public final class AndroidOsBuild {
    private static AndroidOsBuild sOverrideForTesting;

    private final String mType;

    public AndroidOsBuild(String type) {
        mType = type;
    }

    public static AndroidOsBuild get() {
        return sOverrideForTesting != null ? sOverrideForTesting : new AndroidOsBuild(Build.TYPE);
    }

    /** See {@link android.os.Build#TYPE} */
    public String getType() {
        return mType;
    }

    public static final class WithOverrideForTesting implements AutoCloseable {
        public WithOverrideForTesting(AndroidOsBuild override) {
            assert sOverrideForTesting == null;
            sOverrideForTesting = override;
        }

        @Override
        public void close() {
            assert sOverrideForTesting != null;
            sOverrideForTesting = null;
        }
    }
}
