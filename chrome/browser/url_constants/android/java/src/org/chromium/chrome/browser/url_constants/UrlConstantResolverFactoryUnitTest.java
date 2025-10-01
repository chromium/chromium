// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.url_constants;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.when;

import org.junit.After;
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
import org.chromium.components.embedder_support.util.UrlConstants;

/** Unit tests for {@link UrlConstantResolverFactory}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.CHROME_NATIVE_URL_OVERRIDING})
public class UrlConstantResolverFactoryUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;

    @After
    public void tearDown() throws Exception {
        ExtensionsUrlOverrideRegistry.setNtpOverrideEnabled(false);
        ExtensionsUrlOverrideRegistry.setIncognitoNtpOverrideEnabled(false);
        ExtensionsUrlOverrideRegistry.setHistoryPageOverrideEnabled(false);
        ExtensionsUrlOverrideRegistry.setBookmarksPageOverrideEnabled(false);
        ExtensionsUrlOverrideRegistry.setIncognitoBookmarksPageOverrideEnabled(false);
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

        when(mProfile.isIncognitoBranded()).thenReturn(true);
        UrlConstantResolver incognitoResolver = UrlConstantResolverFactory.getForProfile(mProfile);

        assertNotEquals(originalResolver, incognitoResolver);
    }

    @Test
    public void testSetForTesting() {
        UrlConstantResolver testResolver = new UrlConstantResolver();
        UrlConstantResolverFactory.setForTesting(testResolver);

        assertEquals(testResolver, UrlConstantResolverFactory.getForProfile(null));
        assertEquals(testResolver, UrlConstantResolverFactory.getForProfile(mProfile));

        when(mProfile.isIncognitoBranded()).thenReturn(true);
        assertEquals(testResolver, UrlConstantResolverFactory.getForProfile(mProfile));
    }

    @Test
    @DisableFeatures({ChromeFeatureList.CHROME_NATIVE_URL_OVERRIDING})
    public void testOriginalResolver_FeatureDisabled() {
        UrlConstantResolver resolver = UrlConstantResolverFactory.getForProfile(mProfile);
        assertEquals(UrlConstants.NTP_URL, resolver.getNtpUrl());
        assertEquals(UrlConstants.BOOKMARKS_NATIVE_URL, resolver.getBookmarksPageUrl());
        assertEquals(UrlConstants.NATIVE_HISTORY_URL, resolver.getHistoryPageUrl());
    }

    @Test
    public void testOriginalResolver_NtpOverride() {
        UrlConstantResolver resolver = UrlConstantResolverFactory.getForProfile(mProfile);

        ExtensionsUrlOverrideRegistry.setNtpOverrideEnabled(true);
        assertEquals(UrlConstants.NTP_NON_NATIVE_URL, resolver.getNtpUrl());

        ExtensionsUrlOverrideRegistry.setNtpOverrideEnabled(false);
        assertEquals(UrlConstants.NTP_URL, resolver.getNtpUrl());
    }

    @Test
    public void testOriginalResolver_BookmarksOverride() {
        UrlConstantResolver resolver = UrlConstantResolverFactory.getForProfile(mProfile);

        ExtensionsUrlOverrideRegistry.setBookmarksPageOverrideEnabled(true);
        assertEquals(UrlConstants.BOOKMARKS_URL, resolver.getBookmarksPageUrl());

        ExtensionsUrlOverrideRegistry.setBookmarksPageOverrideEnabled(false);
        assertEquals(UrlConstants.BOOKMARKS_NATIVE_URL, resolver.getBookmarksPageUrl());
    }

    @Test
    public void testOriginalResolver_HistoryOverride() {
        UrlConstantResolver resolver = UrlConstantResolverFactory.getForProfile(mProfile);

        ExtensionsUrlOverrideRegistry.setHistoryPageOverrideEnabled(true);
        assertEquals(UrlConstants.HISTORY_URL, resolver.getHistoryPageUrl());

        ExtensionsUrlOverrideRegistry.setHistoryPageOverrideEnabled(false);
        assertEquals(UrlConstants.NATIVE_HISTORY_URL, resolver.getHistoryPageUrl());
    }

    @Test
    @DisableFeatures({ChromeFeatureList.CHROME_NATIVE_URL_OVERRIDING})
    public void testIncognitoResolver_FeatureDisabled() {
        when(mProfile.isIncognitoBranded()).thenReturn(true);
        UrlConstantResolver resolver = UrlConstantResolverFactory.getForProfile(mProfile);
        assertEquals(UrlConstants.NTP_URL, resolver.getNtpUrl());
        assertEquals(UrlConstants.BOOKMARKS_NATIVE_URL, resolver.getBookmarksPageUrl());
        assertEquals(UrlConstants.NATIVE_HISTORY_URL, resolver.getHistoryPageUrl());
    }

    @Test
    public void testIncognitoResolver_NtpOverride() {
        when(mProfile.isIncognitoBranded()).thenReturn(true);
        UrlConstantResolver resolver = UrlConstantResolverFactory.getForProfile(mProfile);

        ExtensionsUrlOverrideRegistry.setIncognitoNtpOverrideEnabled(true);
        assertEquals(UrlConstants.NTP_NON_NATIVE_URL, resolver.getNtpUrl());

        ExtensionsUrlOverrideRegistry.setIncognitoNtpOverrideEnabled(false);
        assertEquals(UrlConstants.NTP_URL, resolver.getNtpUrl());
    }

    @Test
    public void testIncognitoResolver_BookmarksOverride() {
        when(mProfile.isIncognitoBranded()).thenReturn(true);
        UrlConstantResolver resolver = UrlConstantResolverFactory.getForProfile(mProfile);

        ExtensionsUrlOverrideRegistry.setIncognitoBookmarksPageOverrideEnabled(true);
        assertEquals(UrlConstants.BOOKMARKS_URL, resolver.getBookmarksPageUrl());

        ExtensionsUrlOverrideRegistry.setIncognitoBookmarksPageOverrideEnabled(false);
        assertEquals(UrlConstants.BOOKMARKS_NATIVE_URL, resolver.getBookmarksPageUrl());
    }

    @Test
    public void testIncognitoResolver_NoHistoryOverride() {
        when(mProfile.isIncognitoBranded()).thenReturn(true);
        UrlConstantResolver resolver = UrlConstantResolverFactory.getForProfile(mProfile);

        ExtensionsUrlOverrideRegistry.setHistoryPageOverrideEnabled(true);
        assertEquals(UrlConstants.NATIVE_HISTORY_URL, resolver.getHistoryPageUrl());
    }
}
