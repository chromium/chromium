// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.url_constants;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.embedder_support.util.UrlConstants;

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

        if (profile == null || !profile.isIncognitoBranded()) {
            return getOriginalResolver();
        }

        return getIncognitoResolver();
    }

    private static UrlConstantResolver getOriginalResolver() {
        if (sOriginalResolver == null) {
            sOriginalResolver = buildOriginalResolver();
        }
        return sOriginalResolver;
    }

    private static UrlConstantResolver getIncognitoResolver() {
        if (sIncognitoResolver == null) {
            sIncognitoResolver = buildIncognitoResolver();
        }
        return sIncognitoResolver;
    }

    private static UrlConstantResolver buildOriginalResolver() {
        UrlConstantResolver resolver = new UrlConstantResolver();
        if (!ChromeFeatureList.sChromeNativeUrlOverriding.isEnabled()) return resolver;

        resolver.registerOverride(
                UrlConstants.NTP_URL,
                () ->
                        ExtensionsUrlOverrideRegistry.getNtpOverrideEnabled()
                                ? UrlConstants.NTP_NON_NATIVE_URL
                                : null);
        resolver.registerOverride(
                UrlConstants.BOOKMARKS_NATIVE_URL,
                () ->
                        ExtensionsUrlOverrideRegistry.getBookmarksPageOverrideEnabled()
                                ? UrlConstants.BOOKMARKS_URL
                                : null);
        resolver.registerOverride(
                UrlConstants.NATIVE_HISTORY_URL,
                () ->
                        ExtensionsUrlOverrideRegistry.getHistoryPageOverrideEnabled()
                                ? UrlConstants.HISTORY_URL
                                : null);
        return resolver;
    }

    private static UrlConstantResolver buildIncognitoResolver() {
        UrlConstantResolver resolver = new UrlConstantResolver();
        if (!ChromeFeatureList.sChromeNativeUrlOverriding.isEnabled()) return resolver;

        resolver.registerOverride(
                UrlConstants.NTP_URL,
                () ->
                        ExtensionsUrlOverrideRegistry.getIncognitoNtpOverrideEnabled()
                                ? UrlConstants.NTP_NON_NATIVE_URL
                                : null);
        resolver.registerOverride(
                UrlConstants.BOOKMARKS_NATIVE_URL,
                () ->
                        ExtensionsUrlOverrideRegistry.getIncognitoBookmarksPageOverrideEnabled()
                                ? UrlConstants.BOOKMARKS_URL
                                : null);
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
