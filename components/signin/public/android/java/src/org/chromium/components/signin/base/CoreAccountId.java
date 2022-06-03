// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.base;

import androidx.annotation.NonNull;

import org.chromium.base.annotations.CalledByNative;

/**
 * Represents the id of an account, which can be either a Gaia ID or email depending on the
 * migration status within AccountTrackerService.
 * This class has a native counterpart called CoreAccountId.
 */
public class CoreAccountId {
    private final String mId;

    /**
     * Constructs a new CoreAccountId from a String representation of the account ID.
     */
    @CalledByNative
    public CoreAccountId(@NonNull String id) {
        assert id != null;
        mId = id;
    }

    @CalledByNative
    public String getId() {
        return mId;
    }

    @Override
    public String toString() {
        return mId;
    }

    @Override
    public int hashCode() {
        return mId.hashCode();
    }

    @Override
    public boolean equals(Object obj) {
        if (!(obj instanceof CoreAccountId)) return false;
        CoreAccountId other = (CoreAccountId) obj;
        return mId.equals(other.mId);
    }
}
