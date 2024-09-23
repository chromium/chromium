// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browsing_data.content;

import androidx.annotation.VisibleForTesting;

import org.chromium.url.Origin;

import java.io.Serializable;

/** Browsing Data information for a given origin. */
public class BrowsingDataInfo implements Serializable {
    private final Origin mOrigin;
    private final int mCookieCount;
    private final long mStorageSize;
    private final boolean mImportantDomain;

    @VisibleForTesting
    public BrowsingDataInfo(
            Origin origin, int cookieCount, long storageSize, boolean importantDomain) {
        mOrigin = origin;
        mCookieCount = cookieCount;
        mStorageSize = storageSize;
        mImportantDomain = importantDomain;
    }

    public Origin getOrigin() {
        return mOrigin;
    }

    public long getStorageSize() {
        return mStorageSize;
    }

    public int getCookieCount() {
        return mCookieCount;
    }

    public boolean isDomainImportant() {
        return mImportantDomain;
    }
}
