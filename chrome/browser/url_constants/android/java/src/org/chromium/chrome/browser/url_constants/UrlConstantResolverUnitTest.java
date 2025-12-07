// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.url_constants;

import static org.junit.Assert.assertEquals;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.url_constants.UrlConstantResolver.PreNativeGurlHolder;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.url.GURL;

/** Unit tests for {@link UrlConstantResolver}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.CHROME_NATIVE_URL_OVERRIDING)
public class UrlConstantResolverUnitTest {
    private static final String OVERRIDE_URL = "OVERRIDE";
    private UrlConstantResolver mResolver;
    private GURL mNtpGurl;
    private GURL mNativeNtpGurl;

    @Before
    public void setUp() {
        mResolver = new UrlConstantResolver();
        mNtpGurl = new GURL("example.com");
        mNativeNtpGurl = new GURL("native_example.com");
    }

    @Test
    public void testGetNtpUrl_NoOverride() {
        assertEquals(UrlConstants.NTP_URL, mResolver.getNtpUrl());
    }

    @Test
    public void testGetBookmarksPageUrl_NoOverride() {
        assertEquals(UrlConstants.BOOKMARKS_NATIVE_URL, mResolver.getBookmarksPageUrl());
    }

    @Test
    public void testGetHistoryPageUrl_NoOverride() {
        assertEquals(UrlConstants.NATIVE_HISTORY_URL, mResolver.getHistoryPageUrl());
    }

    @Test
    public void testGetNtpUrl_WithOverrideEnabled() {
        mResolver.registerOverride(UrlConstants.NTP_URL, () -> OVERRIDE_URL);
        assertEquals(OVERRIDE_URL, mResolver.getNtpUrl());
    }

    @Test
    public void testGetNtpUrl_WithOverrideDisabled() {
        mResolver.registerOverride(UrlConstants.NTP_URL, () -> null);
        assertEquals(UrlConstants.NTP_URL, mResolver.getNtpUrl());
    }

    @Test
    public void testGetBookmarksPageUrl_WithOverrideEnabled() {
        mResolver.registerOverride(UrlConstants.BOOKMARKS_NATIVE_URL, () -> OVERRIDE_URL);
        assertEquals(OVERRIDE_URL, mResolver.getBookmarksPageUrl());
    }

    @Test
    public void testGetBookmarksPageUrl_WithOverrideDisabled() {
        mResolver.registerOverride(UrlConstants.BOOKMARKS_NATIVE_URL, () -> null);
        assertEquals(UrlConstants.BOOKMARKS_NATIVE_URL, mResolver.getBookmarksPageUrl());
    }

    @Test
    public void testGetHistoryPageUrl_WithOverrideEnabled() {
        mResolver.registerOverride(UrlConstants.NATIVE_HISTORY_URL, () -> OVERRIDE_URL);
        assertEquals(OVERRIDE_URL, mResolver.getHistoryPageUrl());
    }

    @Test
    public void testGetHistoryPageUrl_WithOverrideDisabled() {
        mResolver.registerOverride(UrlConstants.NATIVE_HISTORY_URL, () -> null);
        assertEquals(UrlConstants.NATIVE_HISTORY_URL, mResolver.getHistoryPageUrl());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.CHROME_NATIVE_URL_OVERRIDING)
    public void testAllOverridesEnabled_FeatureDisabled() {
        mResolver.registerOverride(UrlConstants.NTP_URL, () -> OVERRIDE_URL);
        mResolver.registerOverride(UrlConstants.BOOKMARKS_NATIVE_URL, () -> OVERRIDE_URL);
        mResolver.registerOverride(UrlConstants.NATIVE_HISTORY_URL, () -> OVERRIDE_URL);
        assertEquals(UrlConstants.NTP_URL, mResolver.getNtpUrl());
        assertEquals(UrlConstants.BOOKMARKS_NATIVE_URL, mResolver.getBookmarksPageUrl());
        assertEquals(UrlConstants.NATIVE_HISTORY_URL, mResolver.getHistoryPageUrl());
    }

    @Test
    public void testGetNtpGurl_NoOverride() {
        PreNativeGurlHolder holder = new PreNativeGurlHolder(mNativeNtpGurl, mNtpGurl);
        mResolver.registerPreNativeGurl(UrlConstants.NTP_URL, holder);

        assertEquals(mNativeNtpGurl, mResolver.getNtpGurl());
    }

    @Test
    public void testGetNtpGurl_WithOverride() {
        PreNativeGurlHolder holder = new PreNativeGurlHolder(mNativeNtpGurl, mNtpGurl);
        mResolver.registerPreNativeGurl(UrlConstants.NTP_URL, holder);
        mResolver.registerOverride(UrlConstants.NTP_URL, () -> "some_override");

        assertEquals(mNtpGurl, mResolver.getNtpGurl());
    }

    @Test
    public void testGetNtpGurl_NullGurlOverride() {
        PreNativeGurlHolder holder = new PreNativeGurlHolder(mNativeNtpGurl, null);
        mResolver.registerPreNativeGurl(UrlConstants.NTP_URL, holder);
        mResolver.registerOverride(UrlConstants.NTP_URL, () -> "some_override");

        assertEquals(mNativeNtpGurl, mResolver.getNtpGurl());
    }

    @Test(expected = AssertionError.class)
    public void testGetNtpGurl_NotRegistered() {
        mResolver.getNtpGurl();
    }
}
