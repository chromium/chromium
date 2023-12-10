// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import androidx.annotation.VisibleForTesting;

import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge.StorageInfoClearedCallback;
import org.chromium.content_public.browser.BrowserContextHandle;

import java.io.Serializable;

/** Local Storage information for a given origin. */
public class LocalStorageInfo implements Serializable {
    private final String mOrigin;
    private final long mSize;
    private final boolean mImportantDomain;

    @VisibleForTesting
    public LocalStorageInfo(String origin, long size, boolean importantDomain) {
        mOrigin = origin;
        mSize = size;
        mImportantDomain = importantDomain;
    }

    public String getOrigin() {
        return mOrigin;
    }

    public void clear(
            BrowserContextHandle browserContextHandle, StorageInfoClearedCallback callback) {
        // TODO(dullweber): Cookies should call a callback when cleared as well.
        WebsitePreferenceBridgeJni.get().clearCookieData(browserContextHandle, mOrigin);
        WebsitePreferenceBridgeJni.get()
                .clearLocalStorageData(browserContextHandle, mOrigin, callback);
    }

    public long getSize() {
        return mSize;
    }

    public boolean isDomainImportant() {
        return mImportantDomain;
    }
}
