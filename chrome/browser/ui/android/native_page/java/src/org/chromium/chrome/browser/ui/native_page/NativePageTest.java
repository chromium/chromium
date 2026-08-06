// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.native_page;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ui.native_page.NativePage.NativePageType;
import org.chromium.components.extensions.ExtensionsBuildflags;
import org.chromium.url.GURL;

/** Tests public methods in NativePage. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NativePageTest {
    public static class UrlCombo {
        public String url;
        public @NativePageType int expectedType;

        public UrlCombo(String url, @NativePageType int expectedType) {
            this.url = url;
            this.expectedType = expectedType;
        }
    }

    public static final UrlCombo[] VALID_URLS = {
        new UrlCombo("chrome-native://newtab", NativePageType.NTP),
        new UrlCombo("chrome-native://newtab/", NativePageType.NTP),
        new UrlCombo("chrome-native://bookmarks", NativePageType.BOOKMARKS),
        new UrlCombo("chrome-native://bookmarks/", NativePageType.BOOKMARKS),
        new UrlCombo("chrome-native://bookmarks/#245", NativePageType.BOOKMARKS),
        new UrlCombo("chrome-native://recent-tabs", NativePageType.RECENT_TABS),
        new UrlCombo("chrome-native://recent-tabs/", NativePageType.RECENT_TABS),
        new UrlCombo("chrome://history/", NativePageType.HISTORY)
    };

    public static final String[] INVALID_URLS = {
        null,
        "",
        "newtab",
        "newtab@google.com:80",
        "/newtab",
        "://newtab",
        "chrome://",
        "chrome://most_visited",
        "chrome-native://",
        "chrome-native://newtablet",
        "chrome-native://bookmarks-inc",
        "chrome-native://recent_tabs",
        "chrome-native://recent-tabswitcher",
        "chrome-native://most_visited",
        "chrome-native://astronaut",
        "chrome-internal://newtab",
        "french-fries://newtab",
        "http://bookmarks",
        "https://recent-tabs",
        "newtab://recent-tabs",
        "recent-tabs bookmarks",
    };

    public static boolean isValidInIncognito(UrlCombo urlCombo) {
        return urlCombo.expectedType != NativePageType.RECENT_TABS;
    }

    /** Ensures that NativePage.isNativePageUrl() returns true for native page URLs. */
    @Test
    public void testPositiveIsNativePageUrl() {
        for (UrlCombo urlCombo : VALID_URLS) {
            String url = urlCombo.url;
            GURL gurl = new GURL(url);
            Assert.assertTrue(url, NativePage.isNativePageUrl(gurl, false, false));
            if (isValidInIncognito(urlCombo)) {
                Assert.assertTrue(url, NativePage.isNativePageUrl(gurl, true, false));
            }
        }
    }

    /**
     * Ensures that NativePage.isNativePageUrl() returns false for URLs that don't correspond to a
     * native page.
     */
    @Test
    public void testNegativeIsNativePageUrl() {
        for (String invalidUrl : INVALID_URLS) {
            GURL gurl = new GURL(invalidUrl);
            Assert.assertFalse(invalidUrl, NativePage.isNativePageUrl(gurl, false, false));
            Assert.assertFalse(invalidUrl, NativePage.isNativePageUrl(gurl, true, false));
        }
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SETTINGS_IN_TAB)
    @Config(qualifiers = "sw600dp")
    public void testNativePageType_Settings() {
        String url = "chrome://settings";
        GURL gurl = new GURL(url);
        Assert.assertEquals(
                "Settings page should be a native page",
                NativePageType.SETTINGS,
                NativePage.nativePageType(gurl, null, false, false, false));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SETTINGS_IN_TAB)
    @Config(qualifiers = "sw600dp")
    public void testNativePageType_SettingsInIncognito() {
        String url = "chrome://settings";
        GURL gurl = new GURL(url);
        Assert.assertEquals(
                "Settings page should not exist in incognito",
                NativePageType.NONE,
                NativePage.nativePageType(gurl, null, /* isIncognito= */ true, false, false));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SETTINGS_IN_TAB)
    @Config(qualifiers = "sw320dp")
    public void testNativePageType_SettingsOnPhone() {
        String url = "chrome://settings";
        GURL gurl = new GURL(url);
        Assert.assertEquals(
                "Settings page should not be a native page on phone",
                NativePageType.NONE,
                NativePage.nativePageType(gurl, null, false, false, false));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.SETTINGS_IN_TAB)
    @Config(qualifiers = "sw600dp")
    public void testNativePageType_SettingsDisabled() {
        String url = "chrome://settings";
        GURL gurl = new GURL(url);
        Assert.assertEquals(
                "Settings page should not be a native page",
                NativePageType.NONE,
                NativePage.nativePageType(gurl, null, false, false, false));
    }

    @Test
    public void testNativePageType_Pdf() {
        String url1 = "chrome-native://pdf/link?url=xyz";
        String url2 = "chrome-native://pdf/link?url=abc";

        GURL gurl1 = new GURL(url1);

        NativePage candidatePage = mock(NativePage.class);
        final boolean incognito = true;
        final boolean urlTyped = true;
        final boolean loadPdf = true;
        doReturn(url1).when(candidatePage).getUrl();
        Assert.assertEquals(
                "Pdf page should be created for non-pdf -> pdf",
                NativePageType.PDF,
                NativePage.nativePageType(gurl1, candidatePage, !incognito, !urlTyped, loadPdf));

        doReturn(true).when(candidatePage).isPdf();
        doReturn(true).when(candidatePage).shouldReusePage(eq(url1), eq(url1), eq(!urlTyped));
        Assert.assertEquals(
                "Candidate page should be reused for pdf -> pdf",
                NativePageType.CANDIDATE,
                NativePage.nativePageType(gurl1, candidatePage, !incognito, !urlTyped, loadPdf));

        doReturn(false).when(candidatePage).shouldReusePage(eq(url1), eq(url1), eq(!urlTyped));
        Assert.assertEquals(
                "Pdf page should be created for pdf activity restart",
                NativePageType.PDF,
                NativePage.nativePageType(gurl1, candidatePage, !incognito, !urlTyped, loadPdf));

        doReturn(url2).when(candidatePage).getUrl();
        doReturn(true).when(candidatePage).shouldReusePage(eq(url2), eq(url1), eq(!urlTyped));
        Assert.assertEquals(
                "Candidate page should be reused for pdf pages",
                NativePageType.CANDIDATE,
                NativePage.nativePageType(gurl1, candidatePage, !incognito, !urlTyped, loadPdf));

        Assert.assertEquals(
                "Native page should not be created without associated pdf download",
                NativePageType.NONE,
                NativePage.nativePageType(gurl1, candidatePage, !incognito, !urlTyped, !loadPdf));
    }

    @Test
    @DisableFeatures({
        ChromeFeatureList.CHROME_NATIVE_URL_OVERRIDING,
        ChromeFeatureList.MIGRATE_MANAGEMENT_TO_WEBUI_ON_MOBILE
    })
    public void testManagementPage_Mobile_FeatureDisabled() {
        if (ExtensionsBuildflags.ENABLE_EXTENSIONS_CORE) {
            return;
        }
        GURL url = new GURL("chrome://management");
        Assert.assertEquals(
                "Management page should be native on mobile when feature is disabled",
                NativePageType.MANAGEMENT,
                NativePage.nativePageType(url, null, false, false, false));
        Assert.assertTrue(
                "isNativePageUrl should be true on mobile when feature is disabled",
                NativePage.isNativePageUrl(url, false, false));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.CHROME_NATIVE_URL_OVERRIDING)
    @EnableFeatures(ChromeFeatureList.MIGRATE_MANAGEMENT_TO_WEBUI_ON_MOBILE)
    public void testManagementPage_Mobile_FeatureEnabled() {
        if (ExtensionsBuildflags.ENABLE_EXTENSIONS_CORE) {
            return;
        }
        GURL url = new GURL("chrome://management");
        Assert.assertEquals(
                "Management page should be WebUI on mobile when feature is enabled",
                NativePageType.NONE,
                NativePage.nativePageType(url, null, false, false, false));
        Assert.assertFalse(
                "isNativePageUrl should be false on mobile when feature is enabled",
                NativePage.isNativePageUrl(url, false, false));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CHROME_NATIVE_URL_OVERRIDING)
    public void testManagementPage_Desktop() {
        GURL url = new GURL("chrome://management");
        Assert.assertEquals(
                "Management page should be WebUI on desktop",
                NativePageType.NONE,
                NativePage.nativePageType(url, null, false, false, false));
        Assert.assertFalse(
                "isNativePageUrl should be false on desktop",
                NativePage.isNativePageUrl(url, false, false));
    }
}
