// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.url_constants;

import static org.chromium.chrome.browser.url_constants.UrlConstantResolver.getOriginalBookmarksUrl;
import static org.chromium.chrome.browser.url_constants.UrlConstantResolver.getOriginalHistoryUrl;
import static org.chromium.chrome.browser.url_constants.UrlConstantResolver.getOriginalNativeBookmarksUrl;
import static org.chromium.chrome.browser.url_constants.UrlConstantResolver.getOriginalNativeHistoryUrl;
import static org.chromium.chrome.browser.url_constants.UrlConstantResolver.getOriginalNativeNtpUrl;
import static org.chromium.chrome.browser.url_constants.UrlConstantResolver.getOriginalNtpUrl;
import static org.chromium.chrome.browser.url_constants.UrlOverrideUtils.isBookmarksPageOverrideEnabled;
import static org.chromium.chrome.browser.url_constants.UrlOverrideUtils.isHistoryPageOverrideEnabled;
import static org.chromium.chrome.browser.url_constants.UrlOverrideUtils.isIncognitoBookmarksPageOverrideEnabled;
import static org.chromium.chrome.browser.url_constants.UrlOverrideUtils.isIncognitoNtpOverrideEnabled;
import static org.chromium.chrome.browser.url_constants.UrlOverrideUtils.isNtpOverrideEnabled;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * This factory creates and keeps a single ExtensionsUrlOverrideRegistryManager for incognito
 * profiles and non-incognito profiles respectively.
 */
@NullMarked
public class UrlConstantResolverFactory {
    private static @Nullable UrlConstantResolver sOriginalResolver;
    private static @Nullable UrlConstantResolver sIncognitoResolver;
    private static @Nullable UrlConstantResolver sResolverForTesting;

    // Prevent instantiation.
    private UrlConstantResolverFactory() {}

    public static UrlConstantResolver getForProfile(@Nullable Profile profile) {
        if (sResolverForTesting != null) {
            return sResolverForTesting;
        }

        if (profile == null || !profile.isOffTheRecord()) {
            return getOriginalResolver();
        }

        return getIncognitoResolver();
    }

    /** Returns the resolver associated with the primary profile. */
    public static UrlConstantResolver getOriginalResolver() {
        if (sOriginalResolver == null) {
            sOriginalResolver = buildOriginalResolver();
        }
        return sOriginalResolver;
    }

    /** Returns the resolver associated with the incognito profile. */
    public static UrlConstantResolver getIncognitoResolver() {
        if (sIncognitoResolver == null) {
            sIncognitoResolver = buildIncognitoResolver();
        }
        return sIncognitoResolver;
    }

    private static UrlConstantResolver buildOriginalResolver() {
        UrlConstantResolver resolver = new UrlConstantResolver();
        resolver.registerOverride(
                getOriginalNativeNtpUrl(),
                () -> isNtpOverrideEnabled() ? getOriginalNtpUrl() : null);
        resolver.registerOverride(
                getOriginalNativeBookmarksUrl(),
                () -> isBookmarksPageOverrideEnabled() ? getOriginalBookmarksUrl() : null);
        resolver.registerOverride(
                getOriginalNativeHistoryUrl(),
                () -> isHistoryPageOverrideEnabled() ? getOriginalHistoryUrl() : null);
        return resolver;
    }

    private static UrlConstantResolver buildIncognitoResolver() {
        UrlConstantResolver resolver = new UrlConstantResolver();
        resolver.registerOverride(
                getOriginalNativeNtpUrl(),
                () -> isIncognitoNtpOverrideEnabled() ? getOriginalNtpUrl() : null);
        resolver.registerOverride(
                getOriginalNativeBookmarksUrl(),
                () -> isIncognitoBookmarksPageOverrideEnabled() ? getOriginalBookmarksUrl() : null);
        return resolver;
    }

    public static void setForTesting(UrlConstantResolver resolver) {
        sResolverForTesting = resolver;
        ResettersForTesting.register(() -> sResolverForTesting = null);
    }

    @VisibleForTesting
    public static void resetResolvers() {
        sOriginalResolver = null;
        sIncognitoResolver = null;
    }
}
