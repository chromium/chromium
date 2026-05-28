// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.url_constants;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.url.GURL;

import java.util.HashMap;
import java.util.Map;

/**
 * Resolves Chrome native page URL constants with support for dynamic page overrides.
 *
 * <p>This resolver is strictly for handling "chrome-native://" scheme URLs (e.g.,
 * "chrome-native://newtab/"). Do NOT use it to override "chrome://" scheme URLs. "chrome-native://"
 * pages are intercepted early in navigation to render native Android Views, whereas "chrome://"
 * overrides are handled in C++ via new_tab_page_url_handler.cc.
 *
 * <p>The URLs returned by original static methods will always remain strictly constant across app
 * loads, sessions, and experiments. They must never fluctuate dynamically, as they serve as
 * persistent keys for database lookups, navigation logging, and override registries.
 */
@NullMarked
public class UrlConstantResolver {

    public UrlConstantResolver() {}

    /** Represents a URL constant override. */
    @FunctionalInterface
    /*package*/ interface UrlConstantOverride {
        /**
         * Returns a URL constant override if enabled. If the override is not enabled, returns null.
         */
        @Nullable String getUrlOverrideIfEnabled();
    }

    private final Map<String, UrlConstantOverride> mUrlConstantOverrides = new HashMap<>();

    /**
     * Returns the dynamically resolved and potentially overridden URL for the New Tab Page.
     *
     * <p>This is the preferred method for obtaining the NTP URL, and should be used for navigation
     * purposes.
     */
    public String getNtpUrl() {
        return getUrlOverrideIfPresent(getOriginalNativeNtpUrl());
    }

    /**
     * Returns the dynamically resolved and potentially overridden URL for the bookmarks page.
     *
     * <p>This is the preferred method for obtaining the bookmarks URL, and should be used for
     * navigation purposes.
     */
    public String getBookmarksPageUrl() {
        return getUrlOverrideIfPresent(getOriginalNativeBookmarksUrl());
    }

    /**
     * Returns the dynamically resolved and potentially overridden URL for the history page.
     *
     * <p>This is the preferred method for obtaining the history URL, and should be used for
     * navigation purposes.
     */
    public String getHistoryPageUrl() {
        return getUrlOverrideIfPresent(getOriginalNativeHistoryUrl());
    }

    /**
     * Registers an override for this URL string.
     *
     * @param url The URL string to override.
     * @param override The override to register.
     */
    /*package*/ void registerOverride(String url, UrlConstantOverride override) {
        mUrlConstantOverrides.put(url, override);
    }

    /**
     * Returns a {@link GURL} representation of the dynamically resolved NTP URL.
     *
     * <p>This is the preferred method for obtaining the NTP GURL, and should be used for navigation
     * purposes.
     */
    public GURL getNtpGurl() {
        return new GURL(getNtpUrl());
    }

    /**
     * Returns the native URL for the New Tab Page, ignoring any overrides. This should only be used
     * when strictly needing the exact native NTP URL, such as for string comparisons. Otherwise,
     * prefer {@link #getNtpUrl()} to obtain the potentially overridden URL.
     *
     * <p>This will strictly remain constant across all experiments and app loads.
     */
    public static String getOriginalNativeNtpUrl() {
        return UrlConstants.NTP_URL;
    }

    /**
     * Returns the WebUI URL for the New Tab Page, ignoring any overrides. This should only be used
     * when strictly needing the exact non-native WebUI NTP URL. Otherwise, prefer {@link
     * #getNtpUrl()} to obtain the active NTP URL.
     *
     * <p>This will strictly remain constant across all experiments and app loads.
     */
    public static String getOriginalWebUiNtpUrl() {
        return UrlConstants.NEW_TAB_PAGE_URL_LEGACY;
    }

    /**
     * Returns the native URL for the bookmarks page, ignoring any overrides. This should only be
     * used when strictly needing the exact native bookmarks URL, such as for string comparisons.
     * Otherwise, prefer {@link #getBookmarksPageUrl()} to obtain the potentially overridden URL.
     *
     * <p>This will strictly remain constant across all experiments and app loads.
     */
    public static String getOriginalNativeBookmarksUrl() {
        return UrlConstants.BOOKMARKS_NATIVE_URL;
    }

    /**
     * Returns the native URL for the history page, ignoring any overrides. This should only be used
     * when strictly needing the exact native history URL, such as for string comparisons.
     * Otherwise, prefer {@link #getHistoryPageUrl()} to obtain the potentially overridden URL.
     *
     * <p>This will strictly remain constant across all experiments and app loads.
     */
    public static String getOriginalNativeHistoryUrl() {
        return UrlConstants.NATIVE_HISTORY_URL;
    }

    /**
     * Returns the URL for the New Tab Page, ignoring any overrides. This should only be used when
     * strictly needing the exact non-native NTP URL, such as for string comparisons. Otherwise,
     * prefer {@link #getNtpUrl()} to obtain the potentially overridden URL.
     *
     * <p>This will strictly remain constant across all experiments and app loads.
     */
    public static String getOriginalNtpUrl() {
        return UrlConstants.NTP_NON_NATIVE_URL;
    }

    /**
     * Returns the URL for the bookmarks page, ignoring any overrides. This should only be used when
     * strictly needing the exact bookmarks URL, such as for string comparisons. Otherwise, prefer
     * {@link #getBookmarksPageUrl()} to obtain the potentially overridden URL.
     *
     * <p>This will strictly remain constant across all experiments and app loads.
     */
    public static String getOriginalBookmarksUrl() {
        return UrlConstants.BOOKMARKS_URL;
    }

    /**
     * Returns the URL for the history page, ignoring any overrides. This should only be used when
     * strictly needing the exact history URL, such as for string comparisons. Otherwise, prefer
     * {@link #getHistoryPageUrl()} to obtain the potentially overridden URL.
     *
     * <p>This will strictly remain constant across all experiments and app loads.
     */
    public static String getOriginalHistoryUrl() {
        return UrlConstants.HISTORY_URL;
    }

    /**
     * Returns the {@link GURL} for the NTP, ignoring any overrides. This should only be used when
     * strictly needing the exact NTP GURL, such as for GURL comparisons. Otherwise, prefer {@link
     * #getNtpGurl()} to obtain the potentially overridden URL.
     *
     * <p>This will strictly remain constant across all experiments and app loads.
     */
    public static GURL getOriginalNtpGurl() {
        return new GURL(getOriginalNtpUrl());
    }

    private String getUrlOverrideIfPresent(String url) {
        if (!ChromeFeatureList.sChromeNativeUrlOverriding.isEnabled()) return url;

        UrlConstantOverride override = mUrlConstantOverrides.get(url);
        if (override == null) return url;

        String urlOverride = override.getUrlOverrideIfEnabled();
        return urlOverride == null ? url : urlOverride;
    }
}
