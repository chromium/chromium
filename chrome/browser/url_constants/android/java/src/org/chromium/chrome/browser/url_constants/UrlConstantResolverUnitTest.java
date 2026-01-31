// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.url_constants;

import static org.junit.Assert.assertEquals;

import static org.chromium.chrome.browser.url_constants.UrlConstantResolver.getOriginalNativeBookmarksUrl;
import static org.chromium.chrome.browser.url_constants.UrlConstantResolver.getOriginalNativeHistoryUrl;
import static org.chromium.chrome.browser.url_constants.UrlConstantResolver.getOriginalNativeNtpGurl;
import static org.chromium.chrome.browser.url_constants.UrlConstantResolver.getOriginalNativeNtpUrl;
import static org.chromium.chrome.browser.url_constants.UrlConstantResolver.getOriginalNonNativeNtpGurl;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.url_constants.UrlConstantResolver.PreNativeGurlHolder;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
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
        assertEquals(getOriginalNativeNtpUrl(), mResolver.getNtpUrl());
    }

    @Test
    public void testGetBookmarksPageUrl_NoOverride() {
        assertEquals(getOriginalNativeBookmarksUrl(), mResolver.getBookmarksPageUrl());
    }

    @Test
    public void testGetHistoryPageUrl_NoOverride() {
        assertEquals(getOriginalNativeHistoryUrl(), mResolver.getHistoryPageUrl());
    }

    @Test
    public void testGetNtpUrl_WithOverrideEnabled() {
        mResolver.registerOverride(getOriginalNativeNtpUrl(), () -> OVERRIDE_URL);
        assertEquals(OVERRIDE_URL, mResolver.getNtpUrl());
    }

    @Test
    public void testGetNtpUrl_WithOverrideDisabled() {
        mResolver.registerOverride(getOriginalNativeNtpUrl(), () -> null);
        assertEquals(getOriginalNativeNtpUrl(), mResolver.getNtpUrl());
    }

    @Test
    public void testGetBookmarksPageUrl_WithOverrideEnabled() {
        mResolver.registerOverride(getOriginalNativeBookmarksUrl(), () -> OVERRIDE_URL);
        assertEquals(OVERRIDE_URL, mResolver.getBookmarksPageUrl());
    }

    @Test
    public void testGetBookmarksPageUrl_WithOverrideDisabled() {
        mResolver.registerOverride(getOriginalNativeBookmarksUrl(), () -> null);
        assertEquals(getOriginalNativeBookmarksUrl(), mResolver.getBookmarksPageUrl());
    }

    @Test
    public void testGetHistoryPageUrl_WithOverrideEnabled() {
        mResolver.registerOverride(getOriginalNativeHistoryUrl(), () -> OVERRIDE_URL);
        assertEquals(OVERRIDE_URL, mResolver.getHistoryPageUrl());
    }

    @Test
    public void testGetHistoryPageUrl_WithOverrideDisabled() {
        mResolver.registerOverride(getOriginalNativeHistoryUrl(), () -> null);
        assertEquals(getOriginalNativeHistoryUrl(), mResolver.getHistoryPageUrl());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.CHROME_NATIVE_URL_OVERRIDING)
    public void testAllOverridesEnabled_FeatureDisabled() {
        mResolver.registerOverride(getOriginalNativeNtpUrl(), () -> OVERRIDE_URL);
        mResolver.registerOverride(getOriginalNativeBookmarksUrl(), () -> OVERRIDE_URL);
        mResolver.registerOverride(getOriginalNativeHistoryUrl(), () -> OVERRIDE_URL);
        assertEquals(getOriginalNativeNtpUrl(), mResolver.getNtpUrl());
        assertEquals(getOriginalNativeBookmarksUrl(), mResolver.getBookmarksPageUrl());
        assertEquals(getOriginalNativeHistoryUrl(), mResolver.getHistoryPageUrl());
    }

    @Test
    public void testGetNtpGurl_NoOverride() {
        PreNativeGurlHolder holder = new PreNativeGurlHolder(mNativeNtpGurl, mNtpGurl);
        mResolver.registerPreNativeGurl(getOriginalNativeNtpUrl(), holder);

        assertEquals(mNativeNtpGurl, mResolver.getNtpGurl());
    }

    @Test
    public void testGetNtpGurl_WithOverride() {
        PreNativeGurlHolder holder = new PreNativeGurlHolder(mNativeNtpGurl, mNtpGurl);
        mResolver.registerPreNativeGurl(getOriginalNativeNtpUrl(), holder);
        mResolver.registerOverride(getOriginalNativeNtpUrl(), () -> "some_override");

        assertEquals(mNtpGurl, mResolver.getNtpGurl());
    }

    @Test
    public void testGetNtpGurl_NullGurlOverride() {
        PreNativeGurlHolder holder = new PreNativeGurlHolder(mNativeNtpGurl, null);
        mResolver.registerPreNativeGurl(getOriginalNativeNtpUrl(), holder);
        mResolver.registerOverride(getOriginalNativeNtpUrl(), () -> "some_override");

        assertEquals(mNativeNtpGurl, mResolver.getNtpGurl());
    }

    @Test(expected = AssertionError.class)
    public void testGetNtpGurl_NotRegistered() {
        mResolver.getNtpGurl();
    }

    @Test
    public void testGetOriginalNativeNtpGurl() {
        GURL ntpGurl;
        try {
            ntpGurl = getOriginalNativeNtpGurl();
        } catch (GURL.BadSerializerVersionException be) {
            Assert.fail(
                    "Serialized value for native ntpGurl in UrlConstants has a stale serializer"
                            + " version");
            return;
        }

        Assert.assertTrue(ntpGurl.isValid());
        Assert.assertEquals(UrlConstants.NTP_HOST, ntpGurl.getHost());
        Assert.assertEquals(UrlConstants.CHROME_NATIVE_SCHEME, ntpGurl.getScheme());
        Assert.assertTrue(UrlUtilities.isNtpUrl(ntpGurl));
    }

    @Test
    public void testGetOriginalNonNativeNtpGurl() {
        GURL ntpGurl;
        try {
            ntpGurl = getOriginalNonNativeNtpGurl();
        } catch (GURL.BadSerializerVersionException be) {
            Assert.fail(
                    "Serialized value for non-native ntpGurl in UrlConstants has a stale serializer"
                            + " version");
            return;
        }

        Assert.assertTrue(ntpGurl.isValid());
        Assert.assertEquals(UrlConstants.NTP_HOST, ntpGurl.getHost());
        Assert.assertEquals(UrlConstants.CHROME_SCHEME, ntpGurl.getScheme());
        Assert.assertTrue(UrlUtilities.isNtpUrl(ntpGurl));
    }
}
