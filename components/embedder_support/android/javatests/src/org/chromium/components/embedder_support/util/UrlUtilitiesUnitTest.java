// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.util;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
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
        assertTrue(
                UrlUtilities.isHttpOrHttps("https://user:pass@awesome.com:9000/bad-scheme/#fake"));
        assertTrue(UrlUtilities.isHttpOrHttps("http://awesome.example.com/"));
        assertTrue(UrlUtilities.isHttpOrHttps("http:example.com"));
        assertTrue(UrlUtilities.isHttpOrHttps("http:"));
        assertTrue(UrlUtilities.isHttpOrHttps("http:go"));
        assertTrue(UrlUtilities.isHttpOrHttps("https:example.com://looks-invalid-but-not"));
        // The [] in path would trigger java.net.URI to throw URISyntaxException, but works fine in
        // java.net.URL.
        assertTrue(UrlUtilities.isHttpOrHttps("http://foo.bar/has[square].html"));

        assertFalse(UrlUtilities.isHttpOrHttps("example.com"));
        assertFalse(UrlUtilities.isHttpOrHttps("about:awesome"));
        assertFalse(UrlUtilities.isHttpOrHttps("data:data"));
        assertFalse(UrlUtilities.isHttpOrHttps("file://hostname/path/to/file"));
        assertFalse(UrlUtilities.isHttpOrHttps("inline:skates.co.uk"));
        assertFalse(UrlUtilities.isHttpOrHttps("javascript:alert(1)"));

        assertFalse(UrlUtilities.isHttpOrHttps("super:awesome"));
        assertFalse(UrlUtilities.isHttpOrHttps("ftp://https:password@example.com/"));
        assertFalse(UrlUtilities.isHttpOrHttps("ftp://https:password@example.com/?http:#http:"));
        assertFalse(
                UrlUtilities.isHttpOrHttps(
                        "google-search://https:password@example.com/?http:#http:"));
        assertFalse(UrlUtilities.isHttpOrHttps("chrome://http://version"));
        assertFalse(UrlUtilities.isHttpOrHttps(""));
    }

    @Test
    @SmallTest
    public void testStripPath() {
        assertEquals(
                "https://example.com:9000",
                UrlUtilities.stripPath("https://user:pass@example.com:9000/path/#extra"));
        assertEquals(
                "http://awesome.example.com",
                UrlUtilities.stripPath("http://awesome.example.com/?query"));
        assertEquals("http://localhost", UrlUtilities.stripPath("http://localhost/"));
        assertEquals("http://", UrlUtilities.stripPath("http:"));
    }

    @Test
    @SmallTest
    public void testStripScheme() {
        // Only scheme gets stripped.
        assertEquals("cs.chromium.org", UrlUtilities.stripScheme("https://cs.chromium.org"));
        assertEquals("cs.chromium.org", UrlUtilities.stripScheme("http://cs.chromium.org"));
        // If there is no scheme, nothing changes.
        assertEquals("cs.chromium.org", UrlUtilities.stripScheme("cs.chromium.org"));
        // Path is not touched/changed.
        String urlWithPath =
                "code.google.com/p/chromium/codesearch#search"
                        + "/&q=testStripScheme&sq=package:chromium&type=cs";
        assertEquals(urlWithPath, UrlUtilities.stripScheme("https://" + urlWithPath));
        // Beginning and ending spaces get trimmed.
        assertEquals("cs.chromium.org", UrlUtilities.stripScheme("  https://cs.chromium.org  "));
    }

    @Test
    @SmallTest
    @EnableFeatures(EmbedderSupportFeatures.ANDROID_CHROME_SCHEME_NAVIGATION_KILL_SWITCH_NAME)
    public void testIsAcceptedScheme() {
        assertTrue(UrlUtilities.isAcceptedScheme(new GURL("about:awesome")));
        assertTrue(UrlUtilities.isAcceptedScheme(new GURL("data:data")));
        assertTrue(
                UrlUtilities.isAcceptedScheme(
                        new GURL("https://user:pass@awesome.com:9000/bad-scheme/#fake")));
        assertTrue(UrlUtilities.isAcceptedScheme(new GURL("http://awesome.example.com/")));
        assertTrue(UrlUtilities.isAcceptedScheme(new GURL("file://hostname/path/to/file")));
        assertTrue(UrlUtilities.isAcceptedScheme(new GURL("inline:skates.co.uk")));
        assertTrue(UrlUtilities.isAcceptedScheme(new GURL("javascript:alert(1)")));
        assertTrue(UrlUtilities.isAcceptedScheme(new GURL("http://foo.bar/has[square].html")));
        assertTrue(UrlUtilities.isAcceptedScheme(new GURL("chrome://version")));
        assertTrue(UrlUtilities.isAcceptedScheme(new GURL("chrome://http://version")));

        assertFalse(UrlUtilities.isAcceptedScheme(new GURL("super:awesome")));
        assertFalse(UrlUtilities.isAcceptedScheme(new GURL("ftp://https:password@example.com/")));
        assertFalse(
                UrlUtilities.isAcceptedScheme(
                        new GURL("ftp://https:password@example.com/?http:#http:")));
        assertFalse(
                UrlUtilities.isAcceptedScheme(
                        new GURL("google-search://https:password@example.com/?http:#http:")));
        assertFalse(UrlUtilities.isAcceptedScheme(GURL.emptyGURL()));
    }

    @Test
    @SmallTest
    @DisableFeatures(EmbedderSupportFeatures.ANDROID_CHROME_SCHEME_NAVIGATION_KILL_SWITCH_NAME)
    public void testIsAcceptedScheme_featureDisabled() {
        assertFalse(UrlUtilities.isAcceptedScheme(new GURL("chrome://version")));
    }

    @Test
    @SmallTest
    public void testIsDownloadableScheme() {
        assertTrue(UrlUtilities.isDownloadableScheme(new GURL("data:data")));
        assertTrue(
                UrlUtilities.isDownloadableScheme(
                        new GURL("https://user:pass@awesome.com:9000/bad-scheme:#fake:")));
        assertTrue(UrlUtilities.isDownloadableScheme(new GURL("http://awesome.example.com/")));
        assertTrue(
                UrlUtilities.isDownloadableScheme(
                        new GURL("filesystem:https://user:pass@google.com:99/t/foo;bar?q=a#ref")));
        assertTrue(
                UrlUtilities.isDownloadableScheme(new GURL("blob:https://awesome.example.com/")));
        assertTrue(UrlUtilities.isDownloadableScheme(new GURL("file://hostname/path/to/file")));

        assertFalse(UrlUtilities.isDownloadableScheme(new GURL("inline:skates.co.uk")));
        assertFalse(UrlUtilities.isDownloadableScheme(new GURL("javascript:alert(1)")));
        assertFalse(UrlUtilities.isDownloadableScheme(new GURL("about:awesome")));
        assertFalse(UrlUtilities.isDownloadableScheme(new GURL("super:awesome")));
        assertFalse(
                UrlUtilities.isDownloadableScheme(new GURL("ftp://https:password@example.com/")));
        assertFalse(
                UrlUtilities.isDownloadableScheme(
                        new GURL("ftp://https:password@example.com/?http:#http:")));
        assertFalse(
                UrlUtilities.isDownloadableScheme(
                        new GURL("google-search://https:password@example.com/?http:#http:")));
        assertFalse(UrlUtilities.isDownloadableScheme(new GURL("chrome://http://version")));
        assertFalse(UrlUtilities.isDownloadableScheme(GURL.emptyGURL()));
    }

    @Test
    @SmallTest
    public void testIsUrlWithinScope() {
        String scope = "http://www.example.com/sub";
        assertTrue(UrlUtilities.isUrlWithinScope(scope, scope));
        assertTrue(UrlUtilities.isUrlWithinScope(scope + "/path", scope));
        assertTrue(UrlUtilities.isUrlWithinScope(scope + "/a b/path", scope + "/a%20b"));

        assertFalse(UrlUtilities.isUrlWithinScope("https://www.example.com/sub", scope));
        assertFalse(UrlUtilities.isUrlWithinScope(scope, scope + "/inner"));
        assertFalse(UrlUtilities.isUrlWithinScope(scope + "/this", scope + "/different"));
        assertFalse(
                UrlUtilities.isUrlWithinScope("http://awesome.example.com", "http://example.com"));
        assertFalse(
                UrlUtilities.isUrlWithinScope(
                        "https://www.google.com.evil.com", "https://www.google.com"));
    }

    @Test
    @SmallTest
    public void testUrlsMatchIgnoringFragments() {
        String url = "http://www.example.com/path";
        assertTrue(UrlUtilities.urlsMatchIgnoringFragments(url, url));
        assertTrue(UrlUtilities.urlsMatchIgnoringFragments(url + "#fragment", url));
        assertTrue(UrlUtilities.urlsMatchIgnoringFragments(url + "#fragment", url + "#fragment2"));
        assertTrue(
                UrlUtilities.urlsMatchIgnoringFragments(
                        "HTTP://www.example.com/path" + "#fragment", url + "#fragment2"));
        assertFalse(
                UrlUtilities.urlsMatchIgnoringFragments(
                        url + "#fragment", "http://example.com:443/path#fragment"));
    }

    @Test
    @SmallTest
    public void testUrlsFragmentsDiffer() {
        String url = "http://www.example.com/path";
        assertFalse(UrlUtilities.urlsFragmentsDiffer(url, url));
        assertTrue(UrlUtilities.urlsFragmentsDiffer(url + "#fragment", url));
    }

    @Test
    @SmallTest
    public void testIsNtpUrlString() {
        assertTrue(UrlUtilities.isNtpUrl("chrome-native://newtab"));
        assertTrue(UrlUtilities.isNtpUrl("chrome://newtab"));
        assertTrue(UrlUtilities.isNtpUrl("about:newtab"));

        assertFalse(UrlUtilities.isNtpUrl("http://www.example.com"));
        assertFalse(UrlUtilities.isNtpUrl("chrome://history"));
        assertFalse(UrlUtilities.isNtpUrl("chrome-native://newtabz"));
        assertFalse(UrlUtilities.isNtpUrl("newtab"));
        assertFalse(UrlUtilities.isNtpUrl(""));
    }

    @Test
    @SmallTest
    public void testIsNtpUrlGurl() {
        assertTrue(UrlUtilities.isNtpUrl(new GURL("chrome-native://newtab")));
        assertTrue(UrlUtilities.isNtpUrl(new GURL("chrome://newtab")));

        // Note that this intentionally differs from UrlUtilities#isNTPUrl(String) (see comments on
        // method).
        assertFalse(UrlUtilities.isNtpUrl(new GURL("about:newtab")));

        assertFalse(UrlUtilities.isNtpUrl(new GURL("http://www.example.com")));
        assertFalse(UrlUtilities.isNtpUrl(new GURL("chrome://history")));
        assertFalse(UrlUtilities.isNtpUrl(new GURL("chrome-native://newtabz")));
        assertFalse(UrlUtilities.isNtpUrl(new GURL("newtab")));
        assertFalse(UrlUtilities.isNtpUrl(new GURL("")));
    }

    @Test
    @SmallTest
    public void testIsChromeNativeUrl() {
        assertTrue(UrlUtilities.isChromeNativeUrl(new GURL("chrome-native://newtab")));
        assertTrue(UrlUtilities.isChromeNativeUrl(new GURL("chrome-native://bookmarks")));

        assertFalse(UrlUtilities.isChromeNativeUrl(new GURL("")));
        assertFalse(UrlUtilities.isChromeNativeUrl(new GURL("https://www.example.com")));
        assertFalse(UrlUtilities.isChromeNativeUrl(new GURL("chrome://about")));
        assertFalse(UrlUtilities.isChromeNativeUrl(new GURL("chrome://newtab")));
        assertFalse(UrlUtilities.isChromeNativeUrl(new GURL("newtab")));
    }

    @Test
    @SmallTest
    public void testIsTelScheme() {
        assertTrue(UrlUtilities.isTelScheme(new GURL("tel:123456789")));
        assertFalse(UrlUtilities.isTelScheme(new GURL("teltel:123456789")));
        assertFalse(UrlUtilities.isTelScheme(null));
    }

    @Test
    @SmallTest
    public void testGetTelNumber() {
        assertEquals("123456789", UrlUtilities.getTelNumber(new GURL("tel:123456789")));
        assertEquals("", UrlUtilities.getTelNumber(new GURL("about:123456789")));
        assertEquals("", UrlUtilities.getTelNumber(null));
    }

    @Test
    @SmallTest
    public void testEscapeQueryParamValue() {
        assertEquals("foo", UrlUtilities.escapeQueryParamValue("foo", false));
        assertEquals("foo%20bar", UrlUtilities.escapeQueryParamValue("foo bar", false));
        assertEquals("foo%2B%2B", UrlUtilities.escapeQueryParamValue("foo++", false));

        assertEquals("foo", UrlUtilities.escapeQueryParamValue("foo", true));
        assertEquals("foo+bar", UrlUtilities.escapeQueryParamValue("foo bar", true));
        assertEquals("foo%2B%2B", UrlUtilities.escapeQueryParamValue("foo++", true));
    }

    // Note that this just tests the plumbing of the Java code to the native
    // net::GetValueForKeyInQuery function, which is tested much more thoroughly there.
    @Test
    @SmallTest
    public void testGetValueForKeyInQuery() {
        GURL url = new GURL("https://www.example.com/?q1=foo&q2=bar&q11=#q2=notbar&q3=baz");
        assertEquals("foo", UrlUtilities.getValueForKeyInQuery(url, "q1"));
        assertEquals("bar", UrlUtilities.getValueForKeyInQuery(url, "q2"));
        assertEquals("", UrlUtilities.getValueForKeyInQuery(url, "q11"));
        assertNull(UrlUtilities.getValueForKeyInQuery(url, "1"));
        assertNull(UrlUtilities.getValueForKeyInQuery(url, "q3"));
    }
}
