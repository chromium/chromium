// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import androidx.annotation.VisibleForTesting;

import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge.StorageInfoClearedCallback;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.url.Origin;

import java.io.Serializable;

/** Browsing Data information for a given origin. */
public class BrowsingDataInfo implements Serializable {
    private final Origin mOrigin;
    private final int mCookieCount;
    private final long mStorageSize;

    @VisibleForTesting
    public BrowsingDataInfo(Origin origin, int cookieCount, long storageSize) {
        mOrigin = origin;
        mCookieCount = cookieCount;
        mStorageSize = storageSize;
    }

    public Origin getOrigin() {
        return mOrigin;
    }

    public void clear(
            BrowserContextHandle browserContextHandle, StorageInfoClearedCallback callback) {
        // TODO(b/254415177): Implement data deletion through BDM.
    }

    public long getStorageSize() {
        return mStorageSize;
    }

    public int getCookieCount() {
        return mCookieCount;
    }
}
