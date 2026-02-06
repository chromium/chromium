// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.url_constants;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.url_constants.UrlConstantResolver.getOriginalNativeBookmarksUrl;
import static org.chromium.chrome.browser.url_constants.UrlConstantResolver.getOriginalNativeHistoryUrl;
import static org.chromium.chrome.browser.url_constants.UrlConstantResolver.getOriginalNativeNtpUrl;
import static org.chromium.chrome.browser.url_constants.UrlConstantResolver.getOriginalNonNativeBookmarksUrl;
import static org.chromium.chrome.browser.url_constants.UrlConstantResolver.getOriginalNonNativeHistoryUrl;
import static org.chromium.chrome.browser.url_constants.UrlConstantResolver.getOriginalNonNativeNtpUrl;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.url.GURL;

/** Unit tests for {@link UrlConstantResolverFactory}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.CHROME_NATIVE_URL_OVERRIDING})
public class UrlConstantResolverFactoryUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;

    private static final String SERIALIZED_NATIVE_NTP_URL =
            "82,1,true,0,13,0,-1,0,-1,16,6,0,-1,22,1,0,-1,0,-1,false,false,chrome-native://newtab/";
    private static final String SERIALIZED_NTP_URL =
            "73,1,true,0,6,0,-1,0,-1,9,6,0,-1,15,1,0,-1,0,-1,false,false,chrome://newtab/";

    private GURL mNtpGurl;
    private GURL mNativeNtpGurl;

    @Before
    public void setUp() {
        mNtpGurl = deserializeGurlString(SERIALIZED_NTP_URL);
        mNativeNtpGurl = deserializeGurlString(SERIALIZED_NATIVE_NTP_URL);
    }

    private static GURL deserializeGurlString(String serializedGurl) {
        return GURL.deserializeLatestVersionOnly(serializedGurl.replace(',', '\0'));
    }

    @After
    public void tearDown() throws Exception {
        ExtensionsUrlOverrideRegistry.setNtpOverrideEnabled(false);
        ExtensionsUrlOverrideRegistry.setIncognitoNtpOverrideEnabled(false);
        ExtensionsUrlOverrideRegistry.setHistoryPageOverrideEnabled(false);
        ExtensionsUrlOverrideRegistry.setBookmarksPageOverrideEnabled(false);
        ExtensionsUrlOverrideRegistry.setIncognitoBookmarksPageOverrideEnabled(false);
        PolicyUrlOverrideRegistry.resetRegistry();
        UrlConstantResolverFactory.resetResolvers();
    }

    @Test
    public void testGetForProfile_NullProfile() {
        UrlConstantResolver resolver = UrlConstantResolverFactory.getForProfile(null);
        assertNotNull(resolver);
    }

    @Test
    public void testGetForProfile_RegularProfile() {
        UrlConstantResolver originalResolver = UrlConstantResolverFactory.getForProfile(null);
        UrlConstantResolver profileResolver = UrlConstantResolverFactory.getForProfile(mProfile);
        assertEquals(originalResolver, profileResolver);
    }

    @Test
    public void testGetForProfile_IncognitoProfile() {
        UrlConstantResolver originalResolver = UrlConstantResolverFactory.getForProfile(mProfile);

        when(mProfile.isOffTheRecord()).thenReturn(true);
        UrlConstantResolver incognitoResolver = UrlConstantResolverFactory.getForProfile(mProfile);

        assertNotEquals(originalResolver, incognitoResolver);
    }

    @Test
    public void testSetForTesting() {
        UrlConstantResolver testResolver = new UrlConstantResolver();
        UrlConstantResolverFactory.setForTesting(testResolver);

        assertEquals(testResolver, UrlConstantResolverFactory.getForProfile(null));
        assertEquals(testResolver, UrlConstantResolverFactory.getForProfile(mProfile));

        when(mProfile.isOffTheRecord()).thenReturn(true);
        assertEquals(testResolver, UrlConstantResolverFactory.getForProfile(mProfile));
    }

    @Test
    @DisableFeatures({ChromeFeatureList.CHROME_NATIVE_URL_OVERRIDING})
    public void testOriginalResolver_FeatureDisabled() {
        UrlConstantResolver resolver = UrlConstantResolverFactory.getForProfile(mProfile);
        assertEquals(getOriginalNativeNtpUrl(), resolver.getNtpUrl());
        assertEquals(getOriginalNativeBookmarksUrl(), resolver.getBookmarksPageUrl());
        assertEquals(getOriginalNativeHistoryUrl(), resolver.getHistoryPageUrl());
    }

    @Test
    public void testOriginalResolver_NtpOverride() {
        UrlConstantResolver resolver = UrlConstantResolverFactory.getForProfile(mProfile);

        ExtensionsUrlOverrideRegistry.setNtpOverrideEnabled(true);
        assertEquals(getOriginalNonNativeNtpUrl(), resolver.getNtpUrl());

        ExtensionsUrlOverrideRegistry.setNtpOverrideEnabled(false);
        assertEquals(getOriginalNativeNtpUrl(), resolver.getNtpUrl());
    }

    @Test
    public void testOriginalResolver_BookmarksOverride() {
        UrlConstantResolver resolver = UrlConstantResolverFactory.getForProfile(mProfile);

        ExtensionsUrlOverrideRegistry.setBookmarksPageOverrideEnabled(true);
        assertEquals(getOriginalNonNativeBookmarksUrl(), resolver.getBookmarksPageUrl());

        ExtensionsUrlOverrideRegistry.setBookmarksPageOverrideEnabled(false);
        assertEquals(getOriginalNativeBookmarksUrl(), resolver.getBookmarksPageUrl());
    }

    @Test
    public void testOriginalResolver_HistoryOverride() {
        UrlConstantResolver resolver = UrlConstantResolverFactory.getForProfile(mProfile);

        ExtensionsUrlOverrideRegistry.setHistoryPageOverrideEnabled(true);
        assertEquals(getOriginalNonNativeHistoryUrl(), resolver.getHistoryPageUrl());

        ExtensionsUrlOverrideRegistry.setHistoryPageOverrideEnabled(false);
        assertEquals(getOriginalNativeHistoryUrl(), resolver.getHistoryPageUrl());
    }

    @Test
    public void testIncognitoResolver_NtpOverride() {
        when(mProfile.isOffTheRecord()).thenReturn(true);
        UrlConstantResolver resolver = UrlConstantResolverFactory.getForProfile(mProfile);

        ExtensionsUrlOverrideRegistry.setIncognitoNtpOverrideEnabled(true);
        assertEquals(getOriginalNonNativeNtpUrl(), resolver.getNtpUrl());

        ExtensionsUrlOverrideRegistry.setIncognitoNtpOverrideEnabled(false);
        assertEquals(getOriginalNativeNtpUrl(), resolver.getNtpUrl());
    }

    @Test
    public void testIncognitoResolver_BookmarksOverride() {
        when(mProfile.isOffTheRecord()).thenReturn(true);
        UrlConstantResolver resolver = UrlConstantResolverFactory.getForProfile(mProfile);

        ExtensionsUrlOverrideRegistry.setIncognitoBookmarksPageOverrideEnabled(true);
        assertEquals(getOriginalNonNativeBookmarksUrl(), resolver.getBookmarksPageUrl());

        ExtensionsUrlOverrideRegistry.setIncognitoBookmarksPageOverrideEnabled(false);
        assertEquals(getOriginalNativeBookmarksUrl(), resolver.getBookmarksPageUrl());
    }

    @Test
    public void testIncognitoResolver_NoHistoryOverride() {
        when(mProfile.isOffTheRecord()).thenReturn(true);
        UrlConstantResolver resolver = UrlConstantResolverFactory.getForProfile(mProfile);

        ExtensionsUrlOverrideRegistry.setHistoryPageOverrideEnabled(true);
        assertEquals(getOriginalNativeHistoryUrl(), resolver.getHistoryPageUrl());
    }

    @Test
    public void testResolverGetNtpGurl() {
        when(mProfile.isOffTheRecord()).thenReturn(false);
        UrlConstantResolver resolver = UrlConstantResolverFactory.getForProfile(mProfile);

        when(mProfile.isOffTheRecord()).thenReturn(true);
        UrlConstantResolver incognitoResolver = UrlConstantResolverFactory.getForProfile(mProfile);

        assertEquals(mNativeNtpGurl, resolver.getNtpGurl());
        assertEquals(mNativeNtpGurl, incognitoResolver.getNtpGurl());

        ExtensionsUrlOverrideRegistry.setNtpOverrideEnabled(true);
        assertEquals(mNtpGurl, resolver.getNtpGurl());

        ExtensionsUrlOverrideRegistry.setIncognitoNtpOverrideEnabled(true);
        assertEquals(mNtpGurl, incognitoResolver.getNtpGurl());
    }

    @Test
    public void testOriginalResolver_PolicyNtpOverride() {
        UrlConstantResolver resolver = UrlConstantResolverFactory.getForProfile(mProfile);

        PolicyUrlOverrideRegistry.setIsNewTabPageLocationOverriddenByPolicy(true);
        assertEquals(getOriginalNonNativeNtpUrl(), resolver.getNtpUrl());

        PolicyUrlOverrideRegistry.setIsNewTabPageLocationOverriddenByPolicy(false);
        assertEquals(getOriginalNativeNtpUrl(), resolver.getNtpUrl());
    }

    @Test
    public void testOriginalResolver_ExtensionsAndPolicyNtpOverride() {
        UrlConstantResolver resolver = UrlConstantResolverFactory.getForProfile(mProfile);

        ExtensionsUrlOverrideRegistry.setNtpOverrideEnabled(true);
        PolicyUrlOverrideRegistry.setIsNewTabPageLocationOverriddenByPolicy(true);
        assertEquals(getOriginalNonNativeNtpUrl(), resolver.getNtpUrl());

        ExtensionsUrlOverrideRegistry.setNtpOverrideEnabled(false);
        PolicyUrlOverrideRegistry.setIsNewTabPageLocationOverriddenByPolicy(false);
        assertEquals(getOriginalNativeNtpUrl(), resolver.getNtpUrl());
    }

    @Test
    public void testIncognitoResolver_PolicyNtpOverrideIgnored() {
        when(mProfile.isOffTheRecord()).thenReturn(true);
        UrlConstantResolver resolver = UrlConstantResolverFactory.getForProfile(mProfile);

        PolicyUrlOverrideRegistry.setIsNewTabPageLocationOverriddenByPolicy(true);
        // Policy override should not affect incognito NTP if not explicitly enabled for incognito.
        assertEquals(getOriginalNativeNtpUrl(), resolver.getNtpUrl());

        PolicyUrlOverrideRegistry.setIsNewTabPageLocationOverriddenByPolicy(false);
        assertEquals(getOriginalNativeNtpUrl(), resolver.getNtpUrl());
    }
}
