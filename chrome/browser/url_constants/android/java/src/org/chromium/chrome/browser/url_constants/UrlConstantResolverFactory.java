// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.url_constants;

import static org.chromium.chrome.browser.url_constants.UrlConstantResolver.getOriginalNativeBookmarksUrl;
import static org.chromium.chrome.browser.url_constants.UrlConstantResolver.getOriginalNativeHistoryUrl;
import static org.chromium.chrome.browser.url_constants.UrlConstantResolver.getOriginalNativeNtpGurl;
import static org.chromium.chrome.browser.url_constants.UrlConstantResolver.getOriginalNativeNtpUrl;
import static org.chromium.chrome.browser.url_constants.UrlConstantResolver.getOriginalNonNativeBookmarksUrl;
import static org.chromium.chrome.browser.url_constants.UrlConstantResolver.getOriginalNonNativeHistoryUrl;
import static org.chromium.chrome.browser.url_constants.UrlConstantResolver.getOriginalNonNativeNtpGurl;
import static org.chromium.chrome.browser.url_constants.UrlConstantResolver.getOriginalNonNativeNtpUrl;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.url_constants.UrlConstantResolver.PreNativeGurlHolder;

/**
 * This factory creates and keeps a single ExtensionsUrlOverrideRegistryManager for incognito
 * profiles and non-incognito profiles respectively.
 */
@NullMarked
public class UrlConstantResolverFactory {
    private static @Nullable PreNativeGurlHolder sPreNativeNtpGurl;
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

    /**
     * Returns the resolver associated with the primary profile. Should be used for pre-native
     * functionality.
     */
    public static UrlConstantResolver getOriginalResolver() {
        if (sOriginalResolver == null) {
            sOriginalResolver = buildOriginalResolver();
        }
        return sOriginalResolver;
    }

    /**
     * Returns the resolver associated with the incognito profile. Should be used for pre-native
     * functionality.
     */
    public static UrlConstantResolver getIncognitoResolver() {
        if (sIncognitoResolver == null) {
            sIncognitoResolver = buildIncognitoResolver();
        }
        return sIncognitoResolver;
    }

    private static UrlConstantResolver buildOriginalResolver() {
        UrlConstantResolver resolver = new UrlConstantResolver();
        resolver.registerPreNativeGurl(getOriginalNativeNtpUrl(), getPreNativeNtpGurlHolder());

        resolver.registerOverride(
                getOriginalNativeNtpUrl(),
                () ->
                        ExtensionsUrlOverrideRegistry.getNtpOverrideEnabled()
                                ? getOriginalNonNativeNtpUrl()
                                : null);
        resolver.registerOverride(
                getOriginalNativeBookmarksUrl(),
                () ->
                        ExtensionsUrlOverrideRegistry.getBookmarksPageOverrideEnabled()
                                ? getOriginalNonNativeBookmarksUrl()
                                : null);
        resolver.registerOverride(
                getOriginalNativeHistoryUrl(),
                () ->
                        ExtensionsUrlOverrideRegistry.getHistoryPageOverrideEnabled()
                                ? getOriginalNonNativeHistoryUrl()
                                : null);
        return resolver;
    }

    private static UrlConstantResolver buildIncognitoResolver() {
        UrlConstantResolver resolver = new UrlConstantResolver();
        resolver.registerPreNativeGurl(getOriginalNativeNtpUrl(), getPreNativeNtpGurlHolder());

        resolver.registerOverride(
                getOriginalNativeNtpUrl(),
                () ->
                        ExtensionsUrlOverrideRegistry.getIncognitoNtpOverrideEnabled()
                                ? getOriginalNonNativeNtpUrl()
                                : null);
        resolver.registerOverride(
                getOriginalNativeBookmarksUrl(),
                () ->
                        ExtensionsUrlOverrideRegistry.getIncognitoBookmarksPageOverrideEnabled()
                                ? getOriginalNonNativeBookmarksUrl()
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

    private static PreNativeGurlHolder getPreNativeNtpGurlHolder() {
        if (sPreNativeNtpGurl == null) {
            sPreNativeNtpGurl = buildPreNativeNtpGurlHolder();
        }
        return sPreNativeNtpGurl;
    }

    private static PreNativeGurlHolder buildPreNativeNtpGurlHolder() {
        return new PreNativeGurlHolder(getOriginalNativeNtpGurl(), getOriginalNonNativeNtpGurl());
    }
}
