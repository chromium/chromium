// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import androidx.annotation.Nullable;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.url.GURL;

/** Unit tests for {@link UrlBarData}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class UrlBarDataTest {
    @Test
    public void forUrlAndText_nonHttpOrHttps_DisplayTextDiffersFromUrl() {
        var url = new GURL("data:text/html,blah,blah");
        UrlBarData data = UrlBarData.forUrlAndText(url, "data:text/html,blah", "BLAH");
        Assert.assertEquals(url, data.url);
        Assert.assertEquals("data:text/html,blah", data.displayText);
        Assert.assertEquals(0, data.originStartIndex);
        // Ensure that the end index is the length of the display text and not the URL.
        Assert.assertEquals(data.displayText.length(), data.originEndIndex);
    }

    @Test
    public void forUrlAndText_aboutUri_NoSlashes() {
        var aboutUrl = new GURL("about:blank#verylongurl.totallylegit.notsuspicious.url.com");
        UrlBarData data = UrlBarData.forUrlAndText(aboutUrl, aboutUrl.getSpec());
        Assert.assertEquals(aboutUrl, data.url);
        Assert.assertEquals(aboutUrl.getSpec(), data.displayText);
        Assert.assertEquals(0, data.originStartIndex);
        // Ensure that the end index is the length of the display text and not the URL.
        Assert.assertEquals(aboutUrl.getSpec().length(), data.originEndIndex);
    }

    @Test
    public void forUrlAndText_aboutUri_WithSlashes() {
        var aboutUrl = new GURL("about://blank#verylongurl.totallylegit.notsuspicious.url.com");
        UrlBarData data = UrlBarData.forUrlAndText(aboutUrl, aboutUrl.getSpec());
        Assert.assertEquals(aboutUrl, data.url);
        Assert.assertEquals(aboutUrl.getSpec(), data.displayText);
        Assert.assertEquals(0, data.originStartIndex);
        // Ensure that the end index is the length of the display text and not the URL.
        Assert.assertEquals(aboutUrl.getSpec().length(), data.originEndIndex);
    }

    @Test
    public void originSpans() {
        verifyOriginSpan("", null, "");
        verifyOriginSpan("about:blank", null, "about:blank");

        verifyOriginSpan("chrome://flags/", null, "chrome://flags");
        verifyOriginSpan("chrome://flags/?egads", null, "chrome://flags/?egads");

        verifyOriginSpan("http://www.google.com", null, "http://www.google.com");
        verifyOriginSpan("http://www.google.com", null, "http://www.google.com/");
        verifyOriginSpan("http://www.google.com", "/?q=blah", "http://www.google.com/?q=blah");

        verifyOriginSpan("https://www.google.com", null, "https://www.google.com");
        verifyOriginSpan("https://www.google.com", null, "https://www.google.com/");
        verifyOriginSpan("https://www.google.com", "/?q=blah", "https://www.google.com/?q=blah");

        // crbug.com/414990
        String testUrl =
                "https://disneyworld.disney.go.com/special-offers/"
                        + "?CMP=KNC-WDW_FY15_DOM_Q1RO_BR_Gold_SpOffer|G|4141300.RR.AM.01.47"
                        + "&keyword_id=s6JyxRifG_dm|walt%20disney%20world|37174067873|e|1540wwa14043";
        verifyOriginSpan(
                "https://disneyworld.disney.go.com",
                "/special-offers/?CMP=KNC-WDW_FY15_DOM_Q1RO_BR_Gold_SpOffer|G|4141300.RR.AM.01.47"
                        + "&keyword_id=s6JyxRifG_dm|walt%20disney%20world|37174067873|e|"
                        + "1540wwa14043",
                testUrl);

        // crbug.com/415387
        verifyOriginSpan("ftp://example.com/ftp.html", null, "ftp://example.com/ftp.html");

        // crbug.com/447416
        verifyOriginSpan("file:///dev/blah", null, "file:///dev/blah");
        verifyOriginSpan(
                "javascript:window.alert('hello');", null, "javascript:window.alert('hello');");
        verifyOriginSpan(
                "data:text/html;charset=utf-8,Page%201",
                null, "data:text/html;charset=utf-8,Page%201");

        // crbug.com/1080395
        verifyOriginSpan("blob:https://origin", "/GUID", "blob:https://origin/GUID");
        verifyOriginSpan("blob:http://origin", "/GUID", "blob:http://origin/GUID");
        verifyOriginSpan("blob:google.com", "/GUID", "blob:google.com/GUID");

        // crbug.com/1257746
        verifyOriginSpan("content://dev/blah", null, "content://dev/blah");
    }

    // http://crbug/1485446
    @Test
    public void forUrlAndText_missingDisplayTextSchemeDoesNotConfuseHosts() {
        var url = new GURL("https://www.a.com/https://bbb.com/i.htm");
        var displayText = "a.com/https://bbb.com/i.htm";
        UrlBarData data = UrlBarData.forUrlAndText(url, displayText);
        Assert.assertEquals(url, data.url);
        Assert.assertEquals(displayText, data.displayText);
        Assert.assertEquals(0, data.originStartIndex);
        Assert.assertEquals("a.com".length(), data.originEndIndex);
    }

    // http://crbug/1485446
    @Test
    public void forUrlAndText_missingDisplayTextSchemeDoesNotConfusePaths() {
        var url = new GURL("https://www.a.com/?k=v/bbb.com/i.htm");
        var displayText = "a.com/?k=v/bbb.com/i.htm";
        UrlBarData data = UrlBarData.forUrlAndText(url, displayText);
        Assert.assertEquals(url, data.url);
        Assert.assertEquals(displayText, data.displayText);
        Assert.assertEquals(0, data.originStartIndex);
        Assert.assertEquals("a.com".length(), data.originEndIndex);
    }

    @Test
    public void forUrlAndText_portNumberDoesNotConfuseHostForScheme() {
        var url = new GURL("https://https:1234/abcd");
        var displayText = "https:1234/abcd";
        UrlBarData data = UrlBarData.forUrlAndText(url, displayText);
        Assert.assertEquals(url, data.url);
        Assert.assertEquals(displayText, data.displayText);
        Assert.assertEquals(0, data.originStartIndex);
        Assert.assertEquals("https:1234".length(), data.originEndIndex);
    }

    @Test
    public void forUrlAndText_doesNotExtractPathFromUnsupportedSchemes() {
        var url = new GURL("data:google.com/test");
        var displayText = "data:google.com/test";
        UrlBarData data = UrlBarData.forUrlAndText(url, displayText);
        Assert.assertEquals(url, data.url);
        Assert.assertEquals(displayText, data.displayText);
        Assert.assertEquals(0, data.originStartIndex);
        Assert.assertEquals(displayText.length(), data.originEndIndex);
    }

    @Test
    public void forUrlAndText_lookupPathInEligibleBlobScheme() {
        var url = new GURL("blob:https://www.a.com/1234-5678");
        var displayText = "blob:https://www.a.com/1234-5678";
        UrlBarData data = UrlBarData.forUrlAndText(url, displayText);
        Assert.assertEquals(url, data.url);
        Assert.assertEquals(displayText, data.displayText);
        Assert.assertEquals(0, data.originStartIndex);
        Assert.assertEquals("blob:https://www.a.com".length(), data.originEndIndex);
    }

    @Test
    public void forUrlAndText_skipPathInNonEligibleBlobScheme() {
        var url = new GURL("blob:object://www.a.com/1234-5678");
        var displayText = "blob:object://www.a.com/1234-5678";
        UrlBarData data = UrlBarData.forUrlAndText(url, displayText);
        Assert.assertEquals(url, data.url);
        Assert.assertEquals(displayText, data.displayText);
        Assert.assertEquals(0, data.originStartIndex);
        Assert.assertEquals("blob:object:".length(), data.originEndIndex);
    }

    @Test
    public void forUrlAndText_misleadingBlobUrlHandledCorrectly() {
        var url = new GURL("blob:https:///");
        // Pick embedded scheme that is eligible for host/path split.
        var displayText = "blob:https:///";
        UrlBarData data = UrlBarData.forUrlAndText(url, displayText);
        Assert.assertEquals(url, data.url);
        Assert.assertEquals(displayText, data.displayText);
        Assert.assertEquals(0, data.originStartIndex);
        Assert.assertEquals("blob:https:///".length(), data.originEndIndex);
    }

    @Test
    public void forUrlAndText_maliciousHostName() {
        // Captures a corner case variant, where DNS resolves "https" host.
        var url = new GURL("https://https:1234/https://a.com/");
        var displayText = "https:1234/https://a.com/";
        UrlBarData data = UrlBarData.forUrlAndText(url, displayText);
        Assert.assertEquals(url, data.url);
        Assert.assertEquals(displayText, data.displayText);
        Assert.assertEquals(0, data.originStartIndex);
        Assert.assertEquals("https:1234".length(), data.originEndIndex);
    }

    @Test
    public void forUrlAndText_invalidUrl() {
        var displayText = "example.com/https://www.google.com";
        var url = new GURL(displayText); // invalid.
        Assert.assertFalse(url.isValid());
        UrlBarData data = UrlBarData.forUrlAndText(url, displayText);
        Assert.assertEquals(null, data.url);
        Assert.assertEquals(displayText, data.displayText);
        Assert.assertEquals(0, data.originStartIndex);
        Assert.assertEquals(0, data.originEndIndex);
    }

    @Test
    public void forUrlAndText_emptyUrl() {
        var displayText = "example.com/https://www.google.com";
        var url = GURL.emptyGURL();
        Assert.assertFalse(url.isValid());
        UrlBarData data = UrlBarData.forUrlAndText(url, displayText);
        Assert.assertEquals(null, data.url);
        Assert.assertEquals(displayText, data.displayText);
        Assert.assertEquals(0, data.originStartIndex);
        Assert.assertEquals(0, data.originEndIndex);
    }

    private void verifyOriginSpan(
            String expectedOrigin, @Nullable String expectedOriginSuffix, String url) {
        UrlBarData urlBarData = UrlBarData.forUrl(new GURL(url));
        String displayText =
                urlBarData.displayText == null ? "" : urlBarData.displayText.toString();
        Assert.assertEquals(
                expectedOriginSuffix == null
                        ? expectedOrigin
                        : expectedOrigin + expectedOriginSuffix,
                displayText);
        Assert.assertEquals(
                "Original start index, end index did not generate expected origin",
                expectedOrigin,
                displayText.substring(urlBarData.originStartIndex, urlBarData.originEndIndex));
    }
}
