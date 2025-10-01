// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.url_constants;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.embedder_support.util.UrlConstants;

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
}
