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

/** A resolver class for resolving Chrome URL constants. */
@NullMarked
public class UrlConstantResolver {
    private static final String SERIALIZED_NATIVE_NTP_URL =
            "82,1,true,0,13,0,-1,0,-1,16,6,0,-1,22,1,0,-1,0,-1,false,false,chrome-native://newtab/";
    private static final String SERIALIZED_NTP_URL =
            "73,1,true,0,6,0,-1,0,-1,9,6,0,-1,15,1,0,-1,0,-1,false,false,chrome://newtab/";

    private static final GURL NATIVE_NTP_GURL = deserializeGurlString(SERIALIZED_NATIVE_NTP_URL);
    private static final GURL NTP_GURL = deserializeGurlString(SERIALIZED_NTP_URL);

    public UrlConstantResolver() {}

    /** Represents a URL constant override. */
    @FunctionalInterface
    /*package*/ interface UrlConstantOverride {
        /**
         * Returns a URL constant override if enabled. If the override is not enabled, returns null.
         */
        @Nullable String getUrlOverrideIfEnabled();
    }

    /**
     * Used to register any GURLs that are accessible pre-native. The GURLs this contains must be
     * created via deserialization in order to operate prior to Native initialization.
     */
    /*package*/ static class PreNativeGurlHolder {
        public final GURL gurl;
        public final @Nullable GURL gurlOverride;

        public PreNativeGurlHolder(GURL gurl, @Nullable GURL gurlOverride) {
            this.gurl = gurl;
            this.gurlOverride = gurlOverride;
        }
    }

    private final Map<String, UrlConstantOverride> mUrlConstantOverrides = new HashMap<>();
    private final Map<String, PreNativeGurlHolder> mPreNativeGurls = new HashMap<>();

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

    /**
     * Registers a {@link GURL} that must be available pre-native.
     *
     * @param url The URL string to override.
     * @param holder Holds the pre-native available GURL, and its override if any.
     */
    /*package*/ void registerPreNativeGurl(String url, PreNativeGurlHolder holder) {
        mPreNativeGurls.put(url, holder);
    }

    /**
     * Returns a cached GURL representation of {@link UrlConstantResolver#getNtpUrl()}. It is safe
     * to call this method before native is loaded and doing so will not block on native loading
     * completion since a hardcoded, serialized string is used.
     */
    public GURL getNtpGurl() {
        GURL ntpGurl = getPreNativeGurl(getOriginalNativeNtpUrl());
        assert ntpGurl != null;
        return ntpGurl;
    }

    /** Returns the native URL for the New Tab Page, ignoring any overrides. */
    public static String getOriginalNativeNtpUrl() {
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

    /** Returns the non-native URL for the New Tab Page, ignoring any overrides. */
    public static String getOriginalNonNativeNtpUrl() {
        return UrlConstants.NTP_NON_NATIVE_URL;
    }

    /** Returns the non-native URL for the bookmarks page, ignoring any overrides. */
    public static String getOriginalNonNativeBookmarksUrl() {
        return UrlConstants.BOOKMARKS_URL;
    }

    /** Returns the non-native URL for the history page, ignoring any overrides. */
    public static String getOriginalNonNativeHistoryUrl() {
        return UrlConstants.HISTORY_URL;
    }

    /**
     * Returns the non-native {@link GURL} for the native NTP, ignoring any overrides.
     *
     * <p>This is guaranteed to be available pre-native.
     */
    public static GURL getOriginalNativeNtpGurl() {
        return NATIVE_NTP_GURL;
    }

    /**
     * Returns the non-native {@link GURL} for the non-native NTP, ignoring any overrides.
     *
     * <p>This is guaranteed to be available pre-native.
     */
    public static GURL getOriginalNonNativeNtpGurl() {
        return NTP_GURL;
    }

    private String getUrlOverrideIfPresent(String url) {
        if (!ChromeFeatureList.sChromeNativeUrlOverriding.isEnabled()) return url;

        UrlConstantOverride override = mUrlConstantOverrides.get(url);
        if (override == null) return url;

        String urlOverride = override.getUrlOverrideIfEnabled();
        return urlOverride == null ? url : urlOverride;
    }

    /**
     * Returns a {@link GURL} that is guaranteed to be available pre-native, or null if none was
     * registered for the provided URL.
     *
     * @param url The URL string to override. This must be the original, non-overridden URL.
     */
    private @Nullable GURL getPreNativeGurl(String url) {
        PreNativeGurlHolder preNativeGurlHolder = mPreNativeGurls.get(url);
        if (preNativeGurlHolder == null) return null;

        UrlConstantOverride override = mUrlConstantOverrides.get(url);
        GURL gurl = preNativeGurlHolder.gurl;
        if (override == null
                || override.getUrlOverrideIfEnabled() == null
                || !ChromeFeatureList.sChromeNativeUrlOverriding.isEnabled()) {
            return gurl;
        }

        GURL gurlOverride = preNativeGurlHolder.gurlOverride;
        return gurlOverride == null ? gurl : gurlOverride;
    }

    private static GURL deserializeGurlString(String serializedGurl) {
        return GURL.deserializeLatestVersionOnly(serializedGurl.replace(',', '\0'));
    }
}
