// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge.StorageInfoClearedCallback;
import org.chromium.content_public.browser.BrowserContextHandle;

import java.io.Serializable;

/** Shared dictionary information for a given frame origin and a top level site. */
public class SharedDictionaryInfo implements Serializable {
    private final String mOrigin;
    private final String mTopLevelSite;
    private final long mSize;

    public SharedDictionaryInfo(String origin, String topLevelSite, long size) {
        mOrigin = origin;
        mTopLevelSite = topLevelSite;
        mSize = size;
    }

    public String getOrigin() {
        return mOrigin;
    }

    public void clear(
            BrowserContextHandle browserContextHandle, StorageInfoClearedCallback callback) {
        WebsitePreferenceBridgeJni.get()
                .clearSharedDictionary(browserContextHandle, mOrigin, mTopLevelSite, callback);
    }

    public long getSize() {
        return mSize;
    }
}
