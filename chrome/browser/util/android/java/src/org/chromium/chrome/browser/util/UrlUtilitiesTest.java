// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.util;

import android.annotation.SuppressLint;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.content_public.browser.test.NativeLibraryTestRule;

/** Tests for functions in {@link UrlUtilities} that use native code. */
@SuppressLint("Authleak")
@RunWith(BaseJUnit4ClassRunner.class)
public class UrlUtilitiesTest {
    @Rule
    public NativeLibraryTestRule mActivityTestRule = new NativeLibraryTestRule();

    @Before
    public void setUp() {
        mActivityTestRule.loadNativeLibraryNoBrowserProcess();
    }

    @Test
    @SmallTest
    public void testIsAcceptedScheme() {
        Assert.assertTrue(UrlUtilities.isAcceptedScheme("about:awesome"));
        Assert.assertTrue(UrlUtilities.isAcceptedScheme("data:data"));
        Assert.assertTrue(UrlUtilities.isAcceptedScheme(
                "https://user:pass@awesome.com:9000/bad-scheme/#fake"));
        Assert.assertTrue(UrlUtilities.isAcceptedScheme("http://awesome.example.com/"));
        Assert.assertTrue(UrlUtilities.isAcceptedScheme("file://hostname/path/to/file"));
        Assert.assertTrue(UrlUtilities.isAcceptedScheme("inline:skates.co.uk"));
        Assert.assertTrue(UrlUtilities.isAcceptedScheme("javascript:alert(1)"));
        Assert.assertTrue(UrlUtilities.isAcceptedScheme("http://foo.bar/has[square].html"));

        Assert.assertFalse(UrlUtilities.isAcceptedScheme("super:awesome"));
        Assert.assertFalse(UrlUtilities.isAcceptedScheme("ftp://https:password@example.com/"));
        Assert.assertFalse(
                UrlUtilities.isAcceptedScheme("ftp://https:password@example.com/?http:#http:"));
        Assert.assertFalse(UrlUtilities.isAcceptedScheme(
                "google-search://https:password@example.com/?http:#http:"));
        Assert.assertFalse(UrlUtilities.isAcceptedScheme("chrome://http://version"));
        Assert.assertFalse(UrlUtilities.isAcceptedScheme(""));
    }

    @Test
    @SmallTest
    public void testIsDownloadableScheme() {
        Assert.assertTrue(UrlUtilities.isDownloadableScheme("data:data"));
        Assert.assertTrue(UrlUtilities.isDownloadableScheme(
                "https://user:pass@awesome.com:9000/bad-scheme:#fake:"));
        Assert.assertTrue(UrlUtilities.isDownloadableScheme("http://awesome.example.com/"));
        Assert.assertTrue(UrlUtilities.isDownloadableScheme(
                "filesystem:https://user:pass@google.com:99/t/foo;bar?q=a#ref"));
        Assert.assertTrue(UrlUtilities.isDownloadableScheme("blob:https://awesome.example.com/"));
        Assert.assertTrue(UrlUtilities.isDownloadableScheme("file://hostname/path/to/file"));

        Assert.assertFalse(UrlUtilities.isDownloadableScheme("inline:skates.co.uk"));
        Assert.assertFalse(UrlUtilities.isDownloadableScheme("javascript:alert(1)"));
        Assert.assertFalse(UrlUtilities.isDownloadableScheme("about:awesome"));
        Assert.assertFalse(UrlUtilities.isDownloadableScheme("super:awesome"));
        Assert.assertFalse(UrlUtilities.isDownloadableScheme("ftp://https:password@example.com/"));
        Assert.assertFalse(
                UrlUtilities.isDownloadableScheme("ftp://https:password@example.com/?http:#http:"));
        Assert.assertFalse(UrlUtilities.isDownloadableScheme(
                "google-search://https:password@example.com/?http:#http:"));
        Assert.assertFalse(UrlUtilities.isDownloadableScheme("chrome://http://version"));
        Assert.assertFalse(UrlUtilities.isDownloadableScheme(""));
    }

    @Test
    @SmallTest
    public void testIsValidForIntentFallbackUrl() {
        Assert.assertTrue(UrlUtilities.isValidForIntentFallbackNavigation(
                "https://user:pass@awesome.com:9000/bad-scheme:#fake:"));
        Assert.assertTrue(
                UrlUtilities.isValidForIntentFallbackNavigation("http://awesome.example.com/"));
        Assert.assertFalse(UrlUtilities.isValidForIntentFallbackNavigation("inline:skates.co.uk"));
        Assert.assertFalse(UrlUtilities.isValidForIntentFallbackNavigation("javascript:alert(1)"));
        Assert.assertFalse(
                UrlUtilities.isValidForIntentFallbackNavigation("file://hostname/path/to/file"));
        Assert.assertFalse(UrlUtilities.isValidForIntentFallbackNavigation("data:data"));
        Assert.assertFalse(UrlUtilities.isValidForIntentFallbackNavigation("about:awesome"));
        Assert.assertFalse(UrlUtilities.isValidForIntentFallbackNavigation(""));
    }

    @Test
    @SmallTest
    public void testIsUrlWithinScope() {
        String scope = "http://www.example.com/sub";
        Assert.assertTrue(UrlUtilities.isUrlWithinScope(scope, scope));
        Assert.assertTrue(UrlUtilities.isUrlWithinScope(scope + "/path", scope));
        Assert.assertTrue(UrlUtilities.isUrlWithinScope(scope + "/a b/path", scope + "/a%20b"));

        Assert.assertFalse(UrlUtilities.isUrlWithinScope("https://www.example.com/sub", scope));
        Assert.assertFalse(UrlUtilities.isUrlWithinScope(scope, scope + "/inner"));
        Assert.assertFalse(UrlUtilities.isUrlWithinScope(scope + "/this", scope + "/different"));
        Assert.assertFalse(
                UrlUtilities.isUrlWithinScope("http://awesome.example.com", "http://example.com"));
        Assert.assertFalse(UrlUtilities.isUrlWithinScope(
                "https://www.google.com.evil.com", "https://www.google.com"));
    }

    @Test
    @SmallTest
    public void testUrlsMatchIgnoringFragments() {
        String url = "http://www.example.com/path";
        Assert.assertTrue(UrlUtilities.urlsMatchIgnoringFragments(url, url));
        Assert.assertTrue(UrlUtilities.urlsMatchIgnoringFragments(url + "#fragment", url));
        Assert.assertTrue(
                UrlUtilities.urlsMatchIgnoringFragments(url + "#fragment", url + "#fragment2"));
        Assert.assertTrue(UrlUtilities.urlsMatchIgnoringFragments("HTTP://www.example.com/path"
                        + "#fragment",
                url + "#fragment2"));
        Assert.assertFalse(UrlUtilities.urlsMatchIgnoringFragments(
                url + "#fragment", "http://example.com:443/path#fragment"));
    }

    @Test
    @SmallTest
    public void testUrlsFragmentsDiffer() {
        String url = "http://www.example.com/path";
        Assert.assertFalse(UrlUtilities.urlsFragmentsDiffer(url, url));
        Assert.assertTrue(UrlUtilities.urlsFragmentsDiffer(url + "#fragment", url));
    }
}
