// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.url_constants;

import org.chromium.base.CommandLine;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.url.GURL;

import java.util.HashMap;
import java.util.Map;

/** A resolver class for resolving Chrome URL constants. */
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

    /** Returns the potentially overridden URL for the New Tab Page. */
    public String getNtpUrl() {
        return getUrlOverrideIfPresent(getOriginalNativeNtpUrl());
    }

    /** Returns the potentially overridden URL for the bookmarks page. */
    public String getBookmarksPageUrl() {
        return getUrlOverrideIfPresent(getOriginalNativeBookmarksUrl());
    }

    /** Returns the potentially overridden URL for the history page. */
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

    /** Returns a GURL representation of {@link UrlConstantResolver#getNtpUrl()}. */
    public GURL getNtpGurl() {
        return new GURL(getNtpUrl());
    }

    /** Returns the native URL for the New Tab Page, ignoring any overrides. */
    public static String getOriginalNativeNtpUrl() {
        if (CommandLine.getInstance().hasSwitch("use-webui-ntp")) {
            return UrlConstants.NEW_TAB_PAGE_URL_LEGACY;
        }
        return UrlConstants.NTP_URL;
    }

    /** Returns the native URL for the bookmarks page, ignoring any overrides. */
    public static String getOriginalNativeBookmarksUrl() {
        return UrlConstants.BOOKMARKS_NATIVE_URL;
    }

    /** Returns the native URL for the history page, ignoring any overrides. */
    public static String getOriginalNativeHistoryUrl() {
        return UrlConstants.NATIVE_HISTORY_URL;
    }

    /** Returns the URL for the New Tab Page, ignoring any overrides. */
    public static String getOriginalNtpUrl() {
        return UrlConstants.NTP_NON_NATIVE_URL;
    }

    /** Returns the URL for the bookmarks page, ignoring any overrides. */
    public static String getOriginalBookmarksUrl() {
        return UrlConstants.BOOKMARKS_URL;
    }

    /** Returns the URL for the history page, ignoring any overrides. */
    public static String getOriginalHistoryUrl() {
        return UrlConstants.HISTORY_URL;
    }

    /** Returns the {@link GURL} for the NTP, ignoring any overrides. */
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
