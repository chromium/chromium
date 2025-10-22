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
import org.chromium.chrome.browser.url_constants.UrlConstantResolver.PreNativeGurlHolder;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.url.GURL;

/**
 * This factory creates and keeps a single ExtensionsUrlOverrideRegistryManager for incognito
 * profiles and non-incognito profiles respectively.
 */
@NullMarked
public class UrlConstantResolverFactory {
    private static final String SERIALIZED_NATIVE_NTP_URL =
            "82,1,true,0,13,0,-1,0,-1,16,6,0,-1,22,1,0,-1,0,-1,false,false,chrome-native://newtab/";
    private static final String SERIALIZED_NTP_URL =
            "73,1,true,0,6,0,-1,0,-1,9,6,0,-1,15,1,0,-1,0,-1,false,false,chrome://newtab/";

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
        resolver.registerPreNativeGurl(UrlConstants.NTP_URL, getPreNativeNtpGurlHolder());
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
        resolver.registerPreNativeGurl(UrlConstants.NTP_URL, getPreNativeNtpGurlHolder());
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

    private static PreNativeGurlHolder getPreNativeNtpGurlHolder() {
        if (sPreNativeNtpGurl == null) {
            sPreNativeNtpGurl = buildPreNativeNtpGurlHolder();
        }
        return sPreNativeNtpGurl;
    }

    private static PreNativeGurlHolder buildPreNativeNtpGurlHolder() {
        GURL gurlOverride = null;
        if (ChromeFeatureList.sChromeNativeUrlOverriding.isEnabled()) {
            gurlOverride = deserializeGurlString(SERIALIZED_NTP_URL);
        }

        return new PreNativeGurlHolder(
                deserializeGurlString(SERIALIZED_NATIVE_NTP_URL), gurlOverride);
    }

    private static GURL deserializeGurlString(String serializedGurl) {
        return GURL.deserializeLatestVersionOnly(serializedGurl.replace(',', '\0'));
    }
}
