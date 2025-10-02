// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preloading;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.url_constants.UrlConstantResolver;
import org.chromium.chrome.browser.url_constants.UrlConstantResolverFactory;
import org.chromium.url.GURL;

/**
 * Java bridge to handle conditional prerendering using as the user interacts with UI.
 *
 * <p>AndroidPrerenderManager provides interface to start or stop prerendering to the (native).
 */
@NullMarked
public class AndroidPrerenderManager {
    private final long mNativeAndroidPrerenderManager;
    private @Nullable Tab mTab;
    private static @Nullable AndroidPrerenderManager sAndroidPrerenderManager;

    private final TabObserver mTabObserver =
            new EmptyTabObserver() {

                @Override
                public void onContentChanged(Tab tab) {
                    // Opening or navigating back to a new tab page will trigger onContentChanged as
                    // well, resetting variables in this scenario should be avoided.
                    UrlConstantResolver urlConstantResolver =
                            UrlConstantResolverFactory.getForProfile(tab.getProfile());
                    if (tab.getUrl().getSpec().equals(urlConstantResolver.getNtpUrl())
                            || tab.getUrl().getSpec().equals("")) {
                        return;
                    }
                    sAndroidPrerenderManager = new AndroidPrerenderManager();
                    mTab = tab;
                }

                @Override
                public void onDestroyed(Tab tab) {
                    sAndroidPrerenderManager = null;
                    mTab = null;
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
        mNativeAndroidPrerenderManager = AndroidPrerenderManagerJni.get().init();
    }

    /**
     * Link the tab with the AndroidPrerenderManager, and observe the tab.
     *
     * @param tab The tab to be linked with the AndroidPrerenderManager.
     */
    public void initializeWithTab(Tab tab) {
        mTab = tab;
        tab.addObserver(mTabObserver);
    }

    /**
     * Start prerendering with the given URL. This should only be called when
     * AndroidPrerenderManager is properly initialized.
     *
     * @param prerenderUrl The url to be prerendered.
     */
    public void startPrerendering(GURL prerenderUrl) {
        if (mNativeAndroidPrerenderManager == 0 || mTab == null) return;
        AndroidPrerenderManagerJni.get()
                .startPrerendering(mNativeAndroidPrerenderManager, prerenderUrl, mTab);
    }

    /**
     * Stop the started prerendering in the AndroidPrerenderManager instance. This is used to cancel
     * the on-going but stale prerendering.
     */
    public void stopPrerendering() {
        if (mNativeAndroidPrerenderManager == 0 || mTab == null) return;
        AndroidPrerenderManagerJni.get().stopPrerendering(mNativeAndroidPrerenderManager, mTab);
    }

    @NativeMethods
    public interface Natives {
        long init();

        void startPrerendering(
                long nativeAndroidPrerenderManager,
                @JniType("GURL") GURL prerenderUrl,
                @JniType("TabAndroid*") Tab tab);

        void stopPrerendering(long nativeAndroidPrerenderManager, @JniType("TabAndroid*") Tab tab);
    }
}
