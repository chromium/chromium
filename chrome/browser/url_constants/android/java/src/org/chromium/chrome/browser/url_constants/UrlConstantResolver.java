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

    public String getNtpUrl() {
        return getUrlOverrideIfPresent(UrlConstants.NTP_URL);
    }

    public String getBookmarksPageUrl() {
        return getUrlOverrideIfPresent(UrlConstants.BOOKMARKS_NATIVE_URL);
    }

    public String getHistoryPageUrl() {
        return getUrlOverrideIfPresent(UrlConstants.NATIVE_HISTORY_URL);
    }

    private String getUrlOverrideIfPresent(String url) {
        if (!ChromeFeatureList.sChromeNativeUrlOverriding.isEnabled()) return url;

        UrlConstantOverride override = mUrlConstantOverrides.get(url);
        if (override == null) return url;

        String urlOverride = override.getUrlOverrideIfEnabled();
        return urlOverride == null ? url : urlOverride;
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

    /**
     * Returns a cached GURL representation of {@link UrlConstantResolver#getNtpUrl()}. It is safe
     * to call this method before native is loaded and doing so will not block on native loading
     * completion since a hardcoded, serialized string is used.
     */
    public GURL getNtpGurl() {
        GURL ntpGurl = getPreNativeGurl(UrlConstants.NTP_URL);
        assert ntpGurl != null;
        return ntpGurl;
    }
}
