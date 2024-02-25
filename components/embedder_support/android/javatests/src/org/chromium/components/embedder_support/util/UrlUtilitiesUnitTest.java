// Copyright 2017 The Chromium Authors
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
import org.chromium.url.GURL;

/** Unit tests for {@link UrlUtilities}. */
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
        Assert.assertFalse(
                UrlUtilities.isHttpOrHttps(
                        "google-search://https:password@example.com/?http:#http:"));
        Assert.assertFalse(UrlUtilities.isHttpOrHttps("chrome://http://version"));
        Assert.assertFalse(UrlUtilities.isHttpOrHttps(""));
    }

    @Test
    @SmallTest
    public void testStripPath() {
        Assert.assertEquals(
                "https://example.com:9000",
                UrlUtilities.stripPath("https://user:pass@example.com:9000/path/#extra"));
        Assert.assertEquals(
                "http://awesome.example.com",
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
        String urlWithPath =
                "code.google.com/p/chromium/codesearch#search"
                        + "/&q=testStripScheme&sq=package:chromium&type=cs";
        Assert.assertEquals(urlWithPath, UrlUtilities.stripScheme("https://" + urlWithPath));
        // Beginning and ending spaces get trimmed.
        Assert.assertEquals(
                "cs.chromium.org", UrlUtilities.stripScheme("  https://cs.chromium.org  "));
    }

    @Test
    @SmallTest
    public void testIsAcceptedScheme() {
        Assert.assertTrue(UrlUtilities.isAcceptedScheme(new GURL("about:awesome")));
        Assert.assertTrue(UrlUtilities.isAcceptedScheme(new GURL("data:data")));
        Assert.assertTrue(
                UrlUtilities.isAcceptedScheme(
                        new GURL("https://user:pass@awesome.com:9000/bad-scheme/#fake")));
        Assert.assertTrue(UrlUtilities.isAcceptedScheme(new GURL("http://awesome.example.com/")));
        Assert.assertTrue(UrlUtilities.isAcceptedScheme(new GURL("file://hostname/path/to/file")));
        Assert.assertTrue(UrlUtilities.isAcceptedScheme(new GURL("inline:skates.co.uk")));
        Assert.assertTrue(UrlUtilities.isAcceptedScheme(new GURL("javascript:alert(1)")));
        Assert.assertTrue(
                UrlUtilities.isAcceptedScheme(new GURL("http://foo.bar/has[square].html")));

        Assert.assertFalse(UrlUtilities.isAcceptedScheme(new GURL("super:awesome")));
        Assert.assertFalse(
                UrlUtilities.isAcceptedScheme(new GURL("ftp://https:password@example.com/")));
        Assert.assertFalse(
                UrlUtilities.isAcceptedScheme(
                        new GURL("ftp://https:password@example.com/?http:#http:")));
        Assert.assertFalse(
                UrlUtilities.isAcceptedScheme(
                        new GURL("google-search://https:password@example.com/?http:#http:")));
        Assert.assertFalse(UrlUtilities.isAcceptedScheme(new GURL("chrome://http://version")));
        Assert.assertFalse(UrlUtilities.isAcceptedScheme(GURL.emptyGURL()));
    }

    @Test
    @SmallTest
    public void testIsDownloadableScheme() {
        Assert.assertTrue(UrlUtilities.isDownloadableScheme(new GURL("data:data")));
        Assert.assertTrue(
                UrlUtilities.isDownloadableScheme(
                        new GURL("https://user:pass@awesome.com:9000/bad-scheme:#fake:")));
        Assert.assertTrue(
                UrlUtilities.isDownloadableScheme(new GURL("http://awesome.example.com/")));
        Assert.assertTrue(
                UrlUtilities.isDownloadableScheme(
                        new GURL("filesystem:https://user:pass@google.com:99/t/foo;bar?q=a#ref")));
        Assert.assertTrue(
                UrlUtilities.isDownloadableScheme(new GURL("blob:https://awesome.example.com/")));
        Assert.assertTrue(
                UrlUtilities.isDownloadableScheme(new GURL("file://hostname/path/to/file")));

        Assert.assertFalse(UrlUtilities.isDownloadableScheme(new GURL("inline:skates.co.uk")));
        Assert.assertFalse(UrlUtilities.isDownloadableScheme(new GURL("javascript:alert(1)")));
        Assert.assertFalse(UrlUtilities.isDownloadableScheme(new GURL("about:awesome")));
        Assert.assertFalse(UrlUtilities.isDownloadableScheme(new GURL("super:awesome")));
        Assert.assertFalse(
                UrlUtilities.isDownloadableScheme(new GURL("ftp://https:password@example.com/")));
        Assert.assertFalse(
                UrlUtilities.isDownloadableScheme(
                        new GURL("ftp://https:password@example.com/?http:#http:")));
        Assert.assertFalse(
                UrlUtilities.isDownloadableScheme(
                        new GURL("google-search://https:password@example.com/?http:#http:")));
        Assert.assertFalse(UrlUtilities.isDownloadableScheme(new GURL("chrome://http://version")));
        Assert.assertFalse(UrlUtilities.isDownloadableScheme(GURL.emptyGURL()));
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
        Assert.assertFalse(
                UrlUtilities.isUrlWithinScope(
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
        Assert.assertTrue(
                UrlUtilities.urlsMatchIgnoringFragments(
                        "HTTP://www.example.com/path" + "#fragment", url + "#fragment2"));
        Assert.assertFalse(
                UrlUtilities.urlsMatchIgnoringFragments(
                        url + "#fragment", "http://example.com:443/path#fragment"));
    }

    @Test
    @SmallTest
    public void testUrlsFragmentsDiffer() {
        String url = "http://www.example.com/path";
        Assert.assertFalse(UrlUtilities.urlsFragmentsDiffer(url, url));
        Assert.assertTrue(UrlUtilities.urlsFragmentsDiffer(url + "#fragment", url));
    }

    @Test
    @SmallTest
    public void testIsNtpUrlString() {
        Assert.assertTrue(UrlUtilities.isNtpUrl("chrome-native://newtab"));
        Assert.assertTrue(UrlUtilities.isNtpUrl("chrome://newtab"));
        Assert.assertTrue(UrlUtilities.isNtpUrl("about:newtab"));

        Assert.assertFalse(UrlUtilities.isNtpUrl("http://www.example.com"));
        Assert.assertFalse(UrlUtilities.isNtpUrl("chrome://history"));
        Assert.assertFalse(UrlUtilities.isNtpUrl("chrome-native://newtabz"));
        Assert.assertFalse(UrlUtilities.isNtpUrl("newtab"));
        Assert.assertFalse(UrlUtilities.isNtpUrl(""));
    }

    @Test
    @SmallTest
    public void testIsNtpUrlGurl() {
        Assert.assertTrue(UrlUtilities.isNtpUrl(new GURL("chrome-native://newtab")));
        Assert.assertTrue(UrlUtilities.isNtpUrl(new GURL("chrome://newtab")));

        // Note that this intentionally differs from UrlUtilities#isNTPUrl(String) (see comments on
        // method).
        Assert.assertFalse(UrlUtilities.isNtpUrl(new GURL("about:newtab")));

        Assert.assertFalse(UrlUtilities.isNtpUrl(new GURL("http://www.example.com")));
        Assert.assertFalse(UrlUtilities.isNtpUrl(new GURL("chrome://history")));
        Assert.assertFalse(UrlUtilities.isNtpUrl(new GURL("chrome-native://newtabz")));
        Assert.assertFalse(UrlUtilities.isNtpUrl(new GURL("newtab")));
        Assert.assertFalse(UrlUtilities.isNtpUrl(new GURL("")));
    }

    @Test
    @SmallTest
    public void testIsTelScheme() {
        Assert.assertTrue(UrlUtilities.isTelScheme(new GURL("tel:123456789")));
        Assert.assertFalse(UrlUtilities.isTelScheme(new GURL("teltel:123456789")));
        Assert.assertFalse(UrlUtilities.isTelScheme(null));
    }

    @Test
    @SmallTest
    public void testGetTelNumber() {
        Assert.assertEquals("123456789", UrlUtilities.getTelNumber(new GURL("tel:123456789")));
        Assert.assertEquals("", UrlUtilities.getTelNumber(new GURL("about:123456789")));
        Assert.assertEquals("", UrlUtilities.getTelNumber(null));
    }

    @Test
    @SmallTest
    public void testEscapeQueryParamValue() {
        Assert.assertEquals("foo", UrlUtilities.escapeQueryParamValue("foo", false));
        Assert.assertEquals("foo%20bar", UrlUtilities.escapeQueryParamValue("foo bar", false));
        Assert.assertEquals("foo%2B%2B", UrlUtilities.escapeQueryParamValue("foo++", false));

        Assert.assertEquals("foo", UrlUtilities.escapeQueryParamValue("foo", true));
        Assert.assertEquals("foo+bar", UrlUtilities.escapeQueryParamValue("foo bar", true));
        Assert.assertEquals("foo%2B%2B", UrlUtilities.escapeQueryParamValue("foo++", true));
    }

    // Note that this just tests the plumbing of the Java code to the native
    // net::GetValueForKeyInQuery function, which is tested much more thoroughly there.
    @Test
    @SmallTest
    public void testGetValueForKeyInQuery() {
        GURL url = new GURL("https://www.example.com/?q1=foo&q2=bar&q11=#q2=notbar&q3=baz");
        Assert.assertEquals("foo", UrlUtilities.getValueForKeyInQuery(url, "q1"));
        Assert.assertEquals("bar", UrlUtilities.getValueForKeyInQuery(url, "q2"));
        Assert.assertEquals("", UrlUtilities.getValueForKeyInQuery(url, "q11"));
        Assert.assertNull(UrlUtilities.getValueForKeyInQuery(url, "1"));
        Assert.assertNull(UrlUtilities.getValueForKeyInQuery(url, "q3"));
    }
}
