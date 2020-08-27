// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.util;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;

/**
 * Unit tests for {@link UrlUtilities}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
@SuppressWarnings(value = "AuthLeak")
public class UrlUtilitiesUnitTest {
    @Before
    public void setUp() {
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
    }

    @Test
    @SmallTest
    public void testIsHttpOrHttps() {
        Assert.assertTrue(
                UrlUtilities.isHttpOrHttps("https://user:pass@awesome.com:9000/bad-scheme/#fake"));
        Assert.assertTrue(UrlUtilities.isHttpOrHttps("http://awesome.example.com/"));
        Assert.assertTrue(UrlUtilities.isHttpOrHttps("http:example.com"));
        Assert.assertTrue(UrlUtilities.isHttpOrHttps("http:"));
        Assert.assertTrue(UrlUtilities.isHttpOrHttps("http:go"));
        Assert.assertTrue(UrlUtilities.isHttpOrHttps("https:example.com://looks-invalid-but-not"));
        // The [] in path would trigger java.net.URI to throw URISyntaxException, but works fine in
        // java.net.URL.
        Assert.assertTrue(UrlUtilities.isHttpOrHttps("http://foo.bar/has[square].html"));

        Assert.assertFalse(UrlUtilities.isHttpOrHttps("example.com"));
        Assert.assertFalse(UrlUtilities.isHttpOrHttps("about:awesome"));
        Assert.assertFalse(UrlUtilities.isHttpOrHttps("data:data"));
        Assert.assertFalse(UrlUtilities.isHttpOrHttps("file://hostname/path/to/file"));
        Assert.assertFalse(UrlUtilities.isHttpOrHttps("inline:skates.co.uk"));
        Assert.assertFalse(UrlUtilities.isHttpOrHttps("javascript:alert(1)"));

        Assert.assertFalse(UrlUtilities.isHttpOrHttps("super:awesome"));
        Assert.assertFalse(UrlUtilities.isHttpOrHttps("ftp://https:password@example.com/"));
        Assert.assertFalse(
                UrlUtilities.isHttpOrHttps("ftp://https:password@example.com/?http:#http:"));
        Assert.assertFalse(UrlUtilities.isHttpOrHttps(
                "google-search://https:password@example.com/?http:#http:"));
        Assert.assertFalse(UrlUtilities.isHttpOrHttps("chrome://http://version"));
        Assert.assertFalse(UrlUtilities.isHttpOrHttps(""));
    }

    @Test
    @SmallTest
    public void testStripPath() {
        Assert.assertEquals("https://example.com:9000",
                UrlUtilities.stripPath("https://user:pass@example.com:9000/path/#extra"));
        Assert.assertEquals("http://awesome.example.com",
                UrlUtilities.stripPath("http://awesome.example.com/?query"));
        Assert.assertEquals("http://localhost", UrlUtilities.stripPath("http://localhost/"));
        Assert.assertEquals("http://", UrlUtilities.stripPath("http:"));
    }

    @Test
    @SmallTest
    public void testStripScheme() {
        // Only scheme gets stripped.
        Assert.assertEquals("cs.chromium.org", UrlUtilities.stripScheme("https://cs.chromium.org"));
        Assert.assertEquals("cs.chromium.org", UrlUtilities.stripScheme("http://cs.chromium.org"));
        // If there is no scheme, nothing changes.
        Assert.assertEquals("cs.chromium.org", UrlUtilities.stripScheme("cs.chromium.org"));
        // Path is not touched/changed.
        String urlWithPath = "code.google.com/p/chromium/codesearch#search"
                + "/&q=testStripScheme&sq=package:chromium&type=cs";
        Assert.assertEquals(urlWithPath, UrlUtilities.stripScheme("https://" + urlWithPath));
        // Beginning and ending spaces get trimmed.
        Assert.assertEquals(
                "cs.chromium.org", UrlUtilities.stripScheme("  https://cs.chromium.org  "));
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
