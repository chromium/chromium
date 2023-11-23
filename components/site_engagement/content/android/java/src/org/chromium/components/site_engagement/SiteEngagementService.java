// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.site_engagement;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ThreadUtils;
import org.chromium.content_public.browser.BrowserContextHandle;

/**
 * Provides access to the Site Engagement Service for a browser context.
 *
 * Site engagement measures the level of engagement that a user has with an origin. This class
 * allows Java to retrieve and modify engagement scores for URLs.
 */
@JNINamespace("site_engagement")
public class SiteEngagementService {
    /** Pointer to the native side SiteEngagementServiceAndroid shim. */
    private long mNativePointer;

    /**
     * Returns a SiteEngagementService for the provided browser context.
     * Must be called on the UI thread.
     */
    public static SiteEngagementService getForBrowserContext(BrowserContextHandle browserContext) {
        assert ThreadUtils.runningOnUiThread();
        return SiteEngagementServiceJni.get()
                .siteEngagementServiceForBrowserContext(browserContext);
    }

    /**
     * Returns the engagement score for the provided URL.
     * Must be called on the UI thread.
     */
    public double getScore(String url) {
        assert ThreadUtils.runningOnUiThread();
        if (mNativePointer == 0) return 0.0;
        return SiteEngagementServiceJni.get()
                .getScore(mNativePointer, SiteEngagementService.this, url);
    }

    /**
     * Sets the provided URL to have the provided engagement score.
     * Must be called on the UI thread.
     */
    public void resetBaseScoreForUrl(String url, double score) {
        assert ThreadUtils.runningOnUiThread();
        if (mNativePointer == 0) return;
        SiteEngagementServiceJni.get()
                .resetBaseScoreForURL(mNativePointer, SiteEngagementService.this, url, score);
    }

    /** Sets site engagement param values to constants for testing. */
    public static void setParamValuesForTesting() {
        SiteEngagementServiceJni.get().setParamValuesForTesting();
    }

    @CalledByNative
    private static SiteEngagementService create(long nativePointer) {
        return new SiteEngagementService(nativePointer);
    }

    /** This object may only be created via the static getForBrowserContext method. */
    private SiteEngagementService(long nativePointer) {
        mNativePointer = nativePointer;
    }

    @CalledByNative
    private void onNativeDestroyed() {
        mNativePointer = 0;
    }

    @NativeMethods
    interface Natives {
        SiteEngagementService siteEngagementServiceForBrowserContext(
                BrowserContextHandle browserContext);

        void setParamValuesForTesting();

        double getScore(
                long nativeSiteEngagementServiceAndroid, SiteEngagementService caller, String url);

        void resetBaseScoreForURL(
                long nativeSiteEngagementServiceAndroid,
                SiteEngagementService caller,
                String url,
                double score);
    }
}
