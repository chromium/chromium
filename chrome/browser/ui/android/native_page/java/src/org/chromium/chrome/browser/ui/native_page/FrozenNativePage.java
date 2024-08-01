// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.native_page;

import android.view.View;

import androidx.annotation.ColorInt;

/**
 * A empty stand-in for a native page. An inactive NativePage may be replaced with a
 * FrozenNativePage to free up resources.
 *
 * Any method may be called on this object, except for getView(), which will trigger an assert and
 * return null.
 */
public class FrozenNativePage implements NativePage {
    private final String mUrl;
    private final String mHost;
    private final String mTitle;
    private final int mBackgroundColor;
    private final boolean mIsPdf;
    private final String mCanonicalFilepath;
    private final boolean mIsDownloadSafe;

    /** Creates a FrozenNativePage to replace the given NativePage and destroys the NativePage. */
    public static FrozenNativePage freeze(NativePage nativePage) {
        FrozenNativePage fnp = new FrozenNativePage(nativePage);
        nativePage.destroy();
        return fnp;
    }

    private FrozenNativePage(NativePage nativePage) {
        mHost = nativePage.getHost();
        mUrl = nativePage.getUrl();
        mTitle = nativePage.getTitle();
        mBackgroundColor = nativePage.getBackgroundColor();
        mIsPdf = nativePage.isPdf();
        mCanonicalFilepath = nativePage.getCanonicalFilepath();
        mIsDownloadSafe = nativePage.isDownloadSafe();
    }

    @Override
    public View getView() {
        assert false;
        return null;
    }

    @Override
    public String getTitle() {
        return mTitle;
    }

    @Override
    public String getUrl() {
        return mUrl;
    }

    @Override
    public String getHost() {
        return mHost;
    }

    @Override
    public int getBackgroundColor() {
        return mBackgroundColor;
    }

    @Override
    public @ColorInt int getToolbarSceneLayerBackground(@ColorInt int defaultColor) {
        return defaultColor;
    }

    @Override
    public boolean needsToolbarShadow() {
        return true;
    }

    @Override
    public void updateForUrl(String url) {}

    @Override
    public boolean isFrozen() {
        return true;
    }

    @Override
    public boolean isPdf() {
        return mIsPdf;
    }

    @Override
    public String getCanonicalFilepath() {
        return mCanonicalFilepath;
    }

    @Override
    public boolean isDownloadSafe() {
        return mIsDownloadSafe;
    }

    @Override
    public void destroy() {}
}
