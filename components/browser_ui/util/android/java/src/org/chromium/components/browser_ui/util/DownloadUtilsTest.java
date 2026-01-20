// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.util;

import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.embedder_support.util.UrlUtilitiesJni;
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.url.GURL;

/** Unit tests for {@link DownloadUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class DownloadUtilsTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private UrlUtilities.Natives mUrlUtilitiesJniMock;

    @Before
    public void setUp() {
        UrlUtilitiesJni.setInstanceForTesting(mUrlUtilitiesJniMock);
        GURL.ensureNativeInitializedForGURL();
    }

    @Test
    public void testFormatUrlForDisplayInNotification_InvalidOrEmptyUrl() {
        Assert.assertNull(DownloadUtils.formatUrlForDisplayInNotification(null, 100));
        GURL emptyUrl = new GURL("");
        Assert.assertNull(DownloadUtils.formatUrlForDisplayInNotification(emptyUrl, 100));
        GURL invalidUrl = new GURL("foo");
        Assert.assertNull(DownloadUtils.formatUrlForDisplayInNotification(invalidUrl, 100));
    }

    @Test
    public void testFormatUrlForDisplayInNotification_OpaqueOrigin() {
        GURL dataUrl = new GURL("data:text/plain,Hello");
        Assert.assertNull(DownloadUtils.formatUrlForDisplayInNotification(dataUrl, 100));
        GURL bogusSchemeUrl = new GURL("asdf://foo.bar.test");
        Assert.assertNull(DownloadUtils.formatUrlForDisplayInNotification(bogusSchemeUrl, 100));
    }

    @Test
    public void testFormatUrlForDisplayInNotification_FitsUnderLimit() {
        String urlSpec = "https://example.test/path?foo=bar";
        GURL url = new GURL(urlSpec);
        useMockGetDomainAndRegistry(url);
        String expectedFromUrlFormatter = "example.test";

        String urlFormatterOutput =
                UrlFormatter.formatUrlForSecurityDisplay(url, SchemeDisplay.OMIT_HTTP_AND_HTTPS);
        Assert.assertEquals(expectedFromUrlFormatter, urlFormatterOutput);

        String formatted = DownloadUtils.formatUrlForDisplayInNotification(url, urlSpec.length());
        Assert.assertEquals(expectedFromUrlFormatter, formatted);
    }

    @Test
    public void testFormatUrlForDisplayInNotification_ExceedsLimit_FallbackDomainAndRegistryFits() {
        // The non-default port number is included in the output of formatUrlForSecurityDisplay,
        // which makes it too long. However the fallback, which is just the eTLD+1, can still fit.
        String urlSpec = "https://example.test:12345/path?foo=bar";
        GURL url = new GURL(urlSpec);
        useMockGetDomainAndRegistry(url);
        String expectedFromUrlFormatter = "example.test:12345";
        int limit = expectedFromUrlFormatter.length() - 1;
        String expectedFallback = "example.test";

        String urlFormatterOutput =
                UrlFormatter.formatUrlForSecurityDisplay(url, SchemeDisplay.OMIT_HTTP_AND_HTTPS);
        Assert.assertEquals(expectedFromUrlFormatter, urlFormatterOutput);

        // Therefore our formatting function will return the fallback.
        String formatted = DownloadUtils.formatUrlForDisplayInNotification(url, limit);
        Assert.assertEquals(expectedFallback, formatted);

        verify(mUrlUtilitiesJniMock, times(1)).getDomainAndRegistry(anyString(), anyBoolean());
    }

    @Test
    public void
            testFormatUrlForDisplayInNotification_ExceedsLimit_FallbackDomainAndRegistryExceeds() {
        // Same setup as the test above, but with a shorter limit so that the fallback is also too
        // long.
        String urlSpec = "https://example.test:12345/path?foo=bar";
        GURL url = new GURL(urlSpec);
        useMockGetDomainAndRegistry(url);
        String expectedFallback = "example.test";
        int limit = expectedFallback.length() - 1;

        String formatted = DownloadUtils.formatUrlForDisplayInNotification(url, limit);
        Assert.assertNull(formatted);

        verify(mUrlUtilitiesJniMock, times(1)).getDomainAndRegistry(anyString(), anyBoolean());
    }

    @Test
    public void testFormatUrlForDisplayInNotification_ExceedsLimit_FallbackOriginSpecFits() {
        // A URL with empty host that causes the fallback to be the origin spec rather than trying
        // to parse for the eTLD+1.
        String urlSpec = "file:///tmp/path?foo=bar";
        GURL url = new GURL(urlSpec);
        Assert.assertEquals("", url.getOrigin().getHost());
        String expectedFromUrlFormatter = "file:///tmp/path";
        int limit = expectedFromUrlFormatter.length() - 1;
        // Because the fallback is the origin, the path and other components are dropped so this
        // fallback string is shorter.
        String expectedFallback = "file:///";

        String urlFormatterOutput =
                UrlFormatter.formatUrlForSecurityDisplay(url, SchemeDisplay.OMIT_HTTP_AND_HTTPS);
        Assert.assertEquals(expectedFromUrlFormatter, urlFormatterOutput);

        String formatted = DownloadUtils.formatUrlForDisplayInNotification(url, limit);
        Assert.assertEquals(expectedFallback, formatted);

        verify(mUrlUtilitiesJniMock, never()).getDomainAndRegistry(anyString(), anyBoolean());
    }

    @Test
    public void testFormatUrlForDisplayInNotification_ExceedsLimit_FallbackOriginSpecExceeds() {
        // Same setup as the test above, but with a shorter limit so that the fallback is also too
        // long.
        String urlSpec = "file:///tmp/path?foo=bar";
        GURL url = new GURL(urlSpec);
        Assert.assertEquals("", url.getOrigin().getHost());
        String expectedFallback = "file:///";
        int limit = expectedFallback.length() - 1;

        String formatted = DownloadUtils.formatUrlForDisplayInNotification(url, limit);
        Assert.assertNull(formatted);

        verify(mUrlUtilitiesJniMock, never()).getDomainAndRegistry(anyString(), anyBoolean());
    }

    private void useMockGetDomainAndRegistry(GURL url) {
        // Return the host part of the url. This isn't realistic behavior, but is equivalent for the
        // test URLs used above, whose host equals their eTLD+1.
        String host = url.getHost();
        when(mUrlUtilitiesJniMock.getDomainAndRegistry(eq(url.getOrigin().getSpec()), anyBoolean()))
                .then(inv -> host);
    }
}
