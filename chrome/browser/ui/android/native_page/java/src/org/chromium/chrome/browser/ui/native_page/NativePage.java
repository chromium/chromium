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

    /** Updates the native page based on the given url. */
    void updateForUrl(String url);

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

    /** Notify the native page that it is about to be navigated back or hidden by a back press. */
    default void notifyHidingWithBack() {}

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
     * @param isPdf Whether it is a pdf native page.
     * @return Whether the URL would navigate to a native page.
     */
    static boolean isNativePageUrl(GURL url, boolean isIncognito, boolean isPdf) {
        return url != null && nativePageType(url, null, isIncognito, isPdf) != NativePageType.NONE;
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
     * @param isPdf Whether it is a pdf native page.
     * @return Type of the native page defined in {@link NativePageType}.
     */
    // TODO(crbug.com/40549331) - Convert to using GURL.
    static @NativePageType int nativePageType(
            String url, NativePage candidatePage, boolean isIncognito, boolean isPdf) {
        if (url == null) return NativePageType.NONE;

        GURL gurl = new GURL(url);
        return nativePageType(gurl, candidatePage, isIncognito, isPdf);
    }

    /**
     * @param url The URL to be checked.
     * @param candidatePage NativePage to return as result if the url is matched.
     * @param isIncognito Whether the page will be displayed in incognito mode.
     * @param isPdf Whether it is a pdf native page.
     * @return Type of the native page defined in {@link NativePageType}.
     */
    private static @NativePageType int nativePageType(
            GURL url, NativePage candidatePage, boolean isIncognito, boolean isPdf) {
        if (!isPdf) {
            return chromePageType(url, candidatePage, isIncognito);
        }

        if (candidatePage != null && candidatePage.getUrl().equals(url.getSpec())) {
            return NativePageType.CANDIDATE;
        }

        return NativePageType.PDF;
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
