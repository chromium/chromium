// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.native_page;

import android.view.View;

import androidx.annotation.ColorInt;
import androidx.annotation.IntDef;

import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** An interface for pages that will be using Android views instead of html/rendered Web content. */
public interface NativePage {

    /** An interface to trigger the native page's smooth transition. */
    interface SmoothTransitionDelegate {

        /** A callback for delegate to set the initial state of the smooth transition. */
        void prepare();

        /**
         * Start the smooth transition.
         *
         * @param onEnd A runnable to be invoked when the transition is complete.
         */
        void start(Runnable onEnd);

        /** Cancel the smooth transition. */
        void cancel();
    }

    /**
     * @return The View to display the page. This is always non-null.
     */
    View getView();

    /**
     * @return The title of the page.
     */
    String getTitle();

    /**
     * @return The URL of the page.
     */
    String getUrl();

    /**
     * @return The hostname for this page, e.g. "newtab" or "bookmarks".
     */
    String getHost();

    /**
     * @return The background color of the page.
     */
    int getBackgroundColor();

    /**
     * @param defaultColor Default color if not customized.
     * @return The color of the toolbar textbox background.
     */
    default @ColorInt int getToolbarTextBoxBackgroundColor(@ColorInt int defaultColor) {
        return defaultColor;
    }

    /**
     * @param defaultColor Default color if not customized.
     * @return The toolbar (or browser controls) color used in the compositor scene layer.
     * @see {@link Toolbar#getToolbarSceneLayerBackground()}
     */
    default @ColorInt int getToolbarSceneLayerBackground(@ColorInt int defaultColor) {
        return defaultColor;
    }

    /** Reloads the native page. */
    default void reload() {}

    /**
     * @return True if the native page needs the toolbar shadow to be drawn.
     */
    boolean needsToolbarShadow();

    /** Whether the native page supports drawing edge to edge. */
    default boolean supportsEdgeToEdge() {
        return false;
    }

    /** Updates the native page based on the given url. */
    void updateForUrl(String url);

    /** Get the height of the region of the native page view that overlaps top browser controls. */
    default int getHeightOverlappedWithTopControls() {
        return 0;
    }

    /**
     * @return {@code true} if the native page is in inactive/frozen state.
     */
    default boolean isFrozen() {
        return false;
    }

    /**
     * @return {@code true} if the native page is a pdf page.
     */
    default boolean isPdf() {
        return false;
    }

    /**
     * @return the filepath or null if not available. Only pdf native page supports filepath now.
     */
    default String getCanonicalFilepath() {
        return null;
    }

    /**
     * @return {@code true} if the associated download is from secure source or there is no
     *     associated download.
     */
    default boolean isDownloadSafe() {
        return true;
    }

    /** Notify the native page that it is about to be navigated back or hidden by a back press. */
    default void notifyHidingWithBack() {}

    /**
     * Enable the smooth transition for the native page. Defaults to null which means not supported.
     * Return a {@link SmoothTransitionDelegate} which will signal the start and execute the given
     * post-task.
     */
    default SmoothTransitionDelegate enableSmoothTransition() {
        return null;
    }

    /** Called after a page has been removed from the view hierarchy and will no longer be used. */
    void destroy();

    @IntDef({
        NativePageType.NONE,
        NativePageType.CANDIDATE,
        NativePageType.NTP,
        NativePageType.BOOKMARKS,
        NativePageType.RECENT_TABS,
        NativePageType.DOWNLOADS,
        NativePageType.HISTORY,
        NativePageType.EXPLORE,
        NativePageType.MANAGEMENT,
        NativePageType.PDF
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface NativePageType {
        int NONE = 0;
        int CANDIDATE = 1;
        int NTP = 2;
        int BOOKMARKS = 3;
        int RECENT_TABS = 4;
        int DOWNLOADS = 5;
        int HISTORY = 6;
        int EXPLORE = 7;
        int MANAGEMENT = 8;
        int PDF = 9;
    }

    /**
     * @param url The URL to be checked.
     * @param isIncognito Whether the page will be displayed in incognito mode.
     * @param hasPdfDownload Whether the page has an associated pdf download.
     * @return Whether the URL would navigate to a native page.
     */
    static boolean isNativePageUrl(GURL url, boolean isIncognito, boolean hasPdfDownload) {
        return url != null
                && nativePageType(url, null, isIncognito, hasPdfDownload) != NativePageType.NONE;
    }

    /**
     * @param url The URL to be checked.
     * @param isIncognito Whether the page will be displayed in incognito mode.
     * @return Whether the URL would navigate to a native page, excluding pdf native page which do
     *     not have chrome or chrome-native scheme.
     */
    static boolean isChromePageUrl(GURL url, boolean isIncognito) {
        return url != null && chromePageType(url, null, isIncognito) != NativePageType.NONE;
    }

    /**
     * @param url The URL to be checked.
     * @param candidatePage NativePage to return as result if the url is matched.
     * @param isIncognito Whether the page will be displayed in incognito mode.
     * @param hasPdfDownload Whether the page has an associated pdf download.
     * @return Type of the native page defined in {@link NativePageType}.
     */
    // TODO(crbug.com/40549331) - Convert to using GURL.
    static @NativePageType int nativePageType(
            String url, NativePage candidatePage, boolean isIncognito, boolean hasPdfDownload) {
        if (url == null) return NativePageType.NONE;

        GURL gurl = new GURL(url);
        return nativePageType(gurl, candidatePage, isIncognito, hasPdfDownload);
    }

    /**
     * @param url The URL to be checked.
     * @param candidatePage NativePage to return as result if the url is matched.
     * @param isIncognito Whether the page will be displayed in incognito mode.
     * @param hasPdfDownload Whether the page has an associated pdf download.
     * @return Type of the native page defined in {@link NativePageType}.
     */
    private static @NativePageType int nativePageType(
            GURL url, NativePage candidatePage, boolean isIncognito, boolean hasPdfDownload) {
        if (hasPdfDownload) {
            // For navigation with associated pdf download (e.g. open a pdf link), pdf page should
            // be created.
            // Unlike other native pages, each pdf page could be different. We need to compare
            // the entire url instead of the host to determine if the pdf candidate page could
            // be reused.
            if (candidatePage != null && candidatePage.getUrl().equals(url.getSpec())) {
                return NativePageType.CANDIDATE;
            } else {
                return NativePageType.PDF;
            }
        } else if (UrlConstants.PDF_HOST.equals(url.getHost())) {
            // For navigation to chrome-native://pdf/ without associated pdf download (e.g. navigate
            // back/forward to pdf page), do not create pdf page yet. The pdf page will be
            // created after the pdf document is re-downloaded in other parts of the code.
            return NativePageType.NONE;
        } else {
            return chromePageType(url, candidatePage, isIncognito);
        }
    }

    /**
     * @param url The URL to be checked.
     * @param candidatePage NativePage to return as result if the host is matched.
     * @param isIncognito Whether the page will be displayed in incognito mode.
     * @return Type of the native page defined in {@link NativePageType}, excluding pdf native page
     *     which do not have chrome or chrome-native scheme.
     */
    private static @NativePageType int chromePageType(
            GURL url, NativePage candidatePage, boolean isIncognito) {
        String host = url.getHost();
        String scheme = url.getScheme();
        if (!UrlConstants.CHROME_NATIVE_SCHEME.equals(scheme)
                && !UrlConstants.CHROME_SCHEME.equals(scheme)) {
            return NativePageType.NONE;
        }

        if (candidatePage != null && candidatePage.getHost().equals(host)) {
            return NativePageType.CANDIDATE;
        }

        if (UrlConstants.NTP_HOST.equals(host)) {
            return NativePageType.NTP;
        } else if (UrlConstants.BOOKMARKS_HOST.equals(host)) {
            return NativePageType.BOOKMARKS;
        } else if (UrlConstants.DOWNLOADS_HOST.equals(host)) {
            return NativePageType.DOWNLOADS;
        } else if (UrlConstants.HISTORY_HOST.equals(host)) {
            return NativePageType.HISTORY;
        } else if (UrlConstants.RECENT_TABS_HOST.equals(host) && !isIncognito) {
            return NativePageType.RECENT_TABS;
        } else if (UrlConstants.EXPLORE_HOST.equals(host)) {
            return NativePageType.EXPLORE;
        } else if (UrlConstants.MANAGEMENT_HOST.equals(host)) {
            return NativePageType.MANAGEMENT;
        } else {
            return NativePageType.NONE;
        }
    }
}
