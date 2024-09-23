// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preloading;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

/**
 * Java bridge to handle conditional prerendering using as the user interacts with UI.
 *
 * <p>AndroidPrerenderManager provides interface to start or stop prerendering to the (native).
 */
public class AndroidPrerenderManager {
    private long mNativeAndroidPrerenderManager;
    private WebContents mWebContents;
    private static AndroidPrerenderManager sAndroidPrerenderManager;

    private final TabObserver mTabObserver =
            new EmptyTabObserver() {

                @Override
                public void onContentChanged(Tab tab) {
                    // Opening or navigating back to a new tab page will trigger onContentChanged as
                    // well, resetting variables in this scenario should be avoided.
                    if (tab.getUrl().getSpec().equals(UrlConstants.NTP_URL)
                            || tab.getUrl().getSpec().equals("")) {
                        return;
                    }
                    sAndroidPrerenderManager = null;
                    mWebContents = null;
                }

                @Override
                public void onDestroyed(Tab tab) {
                    sAndroidPrerenderManager = null;
                    mWebContents = null;
                }
            };

    public static AndroidPrerenderManager getAndroidPrerenderManager() {
        assert ThreadUtils.runningOnUiThread()
                : "AndroidPrerenderManager should only be called on the UIThread";
        if (sAndroidPrerenderManager == null) {
            sAndroidPrerenderManager = new AndroidPrerenderManager();
        }
        return sAndroidPrerenderManager;
    }

    public static void setAndroidPrerenderManagerForTesting(
            AndroidPrerenderManager androidPrerenderManager) {
        sAndroidPrerenderManager = androidPrerenderManager;
    }

    public static void clearAndroidPrerenderManagerForTesting() {
        sAndroidPrerenderManager = null;
    }

    private AndroidPrerenderManager() {
        mNativeAndroidPrerenderManager = AndroidPrerenderManagerJni.get().init(this);
    }

    /**
     * Link the webContents from the tab with the AndroidPrerenderManager, and observe the tab.
     *
     * @param tab The tab to be linked with the AndroidPrerenderManager.
     */
    public void initializeWithTab(Tab tab) {
        mWebContents = tab.getWebContents();
        tab.addObserver(mTabObserver);
    }

    /**
     * Start prerendering with the given URL. This should only be called when
     * AndroidPrerenderManager is properly initialized.
     *
     * @param prerenderUrl The url to be prerendered.
     * @return Whether prerendering has been started successfully.
     */
    public boolean startPrerendering(GURL prerenderUrl) {
        if (mNativeAndroidPrerenderManager == 0 || mWebContents == null) return false;
        return AndroidPrerenderManagerJni.get()
                .startPrerendering(mNativeAndroidPrerenderManager, prerenderUrl, mWebContents);
    }

    /**
     * Stop the started prerendering in the AndroidPrerenderManager instance. This is used to cancel
     * the on-going but stale prerendering.
     */
    public void stopPrerendering() {
        if (mNativeAndroidPrerenderManager == 0 || mWebContents == null) return;
        AndroidPrerenderManagerJni.get()
                .stopPrerendering(mNativeAndroidPrerenderManager, mWebContents);
    }

    @NativeMethods
    public interface Natives {
        long init(AndroidPrerenderManager caller);

        boolean startPrerendering(
                long nativeAndroidPrerenderManager,
                @JniType("GURL") GURL prerenderUrl,
                WebContents webContents);

        void stopPrerendering(long nativeAndroidPrerenderManager, WebContents webContents);
    }
}
