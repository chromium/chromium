// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.native_page;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ui.native_page.NativePage.NativePageType;

/**
 * Tests public methods in NativePage.
 */
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
            new UrlCombo("chrome://history/", NativePageType.HISTORY)};

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

    /**
     * Ensures that NativePage.isNativePageUrl() returns true for native page URLs.
     */
    @Test
    public void testPositiveIsNativePageUrl() {
        for (UrlCombo urlCombo : VALID_URLS) {
            String url = urlCombo.url;
            Assert.assertTrue(url, NativePage.isNativePageUrl(url, false));
            if (isValidInIncognito(urlCombo)) {
                Assert.assertTrue(url, NativePage.isNativePageUrl(url, true));
            }
        }
    }

    /**
     * Ensures that NativePage.isNativePageUrl() returns false for URLs that don't
     * correspond to a native page.
     */
    @Test
    public void testNegativeIsNativePageUrl() {
        for (String invalidUrl : INVALID_URLS) {
            Assert.assertFalse(invalidUrl, NativePage.isNativePageUrl(invalidUrl, false));
            Assert.assertFalse(invalidUrl, NativePage.isNativePageUrl(invalidUrl, true));
        }
    }
}
