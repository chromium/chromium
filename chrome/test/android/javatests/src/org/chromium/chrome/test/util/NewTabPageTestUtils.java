// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

import org.hamcrest.Matchers;

import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.ntp.IncognitoNewTabPage;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.tile.TileSectionType;
import org.chromium.chrome.browser.suggestions.tile.TileSource;
import org.chromium.chrome.browser.suggestions.tile.TileTitleSource;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/** Utilities for testing the NTP. */
public class NewTabPageTestUtils {
    private static final String TEST_PAGE = "/chrome/test/data/android/navigate/simple.html";

    /**
     * Waits for the NTP owned by the passed in tab to be fully loaded.
     *
     * @param tab The tab to be monitored for NTP loading.
     */
    public static void waitForNtpLoaded(final Tab tab) {
        CriteriaHelper.pollUiThread(
                () -> {
                    if (!tab.isIncognito()) {
                        Criteria.checkThat(
                                tab.getNativePage(), Matchers.instanceOf(NewTabPage.class));
                        Criteria.checkThat(
                                ((NewTabPage) tab.getNativePage()).isLoadedForTests(),
                                Matchers.is(true));
                    } else {
                        Criteria.checkThat(
                                tab.getNativePage(),
                                Matchers.instanceOf(IncognitoNewTabPage.class));
                        Criteria.checkThat(
                                ((IncognitoNewTabPage) tab.getNativePage()).isLoadedForTests(),
                                Matchers.is(true));
                    }
                });
    }

    public static List<SiteSuggestion> createFakeSiteSuggestions(EmbeddedTestServer testServer) {
        List<SiteSuggestion> siteSuggestions = new ArrayList<>();
        siteSuggestions.add(
                new SiteSuggestion(
                        "0 TOP_SITES",
                        new GURL(testServer.getURL(TEST_PAGE) + "#0"),
                        TileTitleSource.TITLE_TAG,
                        TileSource.TOP_SITES,
                        TileSectionType.PERSONALIZED));
        siteSuggestions.add(
                new SiteSuggestion(
                        "1 ALLOWLIST",
                        new GURL(testServer.getURL(TEST_PAGE) + "#1"),
                        TileTitleSource.UNKNOWN,
                        TileSource.ALLOWLIST,
                        TileSectionType.PERSONALIZED));
        siteSuggestions.add(
                new SiteSuggestion(
                        "2 TOP_SITES",
                        new GURL(testServer.getURL(TEST_PAGE) + "#2"),
                        TileTitleSource.TITLE_TAG,
                        TileSource.TOP_SITES,
                        TileSectionType.PERSONALIZED));
        siteSuggestions.add(
                new SiteSuggestion(
                        "3 TOP_SITES",
                        new GURL(testServer.getURL(TEST_PAGE) + "#3"),
                        TileTitleSource.TITLE_TAG,
                        TileSource.TOP_SITES,
                        TileSectionType.PERSONALIZED));
        siteSuggestions.add(
                new SiteSuggestion(
                        "4 TOP_SITES",
                        new GURL(testServer.getURL(TEST_PAGE) + "#4"),
                        TileTitleSource.TITLE_TAG,
                        TileSource.TOP_SITES,
                        TileSectionType.PERSONALIZED));
        siteSuggestions.add(
                new SiteSuggestion(
                        "5 TOP_SITES",
                        new GURL(testServer.getURL(TEST_PAGE) + "#5"),
                        TileTitleSource.TITLE_TAG,
                        TileSource.TOP_SITES,
                        TileSectionType.PERSONALIZED));
        siteSuggestions.add(
                new SiteSuggestion(
                        "6 TOP_SITES",
                        new GURL(testServer.getURL(TEST_PAGE) + "#6"),
                        TileTitleSource.TITLE_TAG,
                        TileSource.TOP_SITES,
                        TileSectionType.PERSONALIZED));
        siteSuggestions.add(
                new SiteSuggestion(
                        "7 TOP_SITES",
                        new GURL(testServer.getURL(TEST_PAGE) + "#7"),
                        TileTitleSource.TITLE_TAG,
                        TileSource.TOP_SITES,
                        TileSectionType.PERSONALIZED));
        return siteSuggestions;
    }
}
