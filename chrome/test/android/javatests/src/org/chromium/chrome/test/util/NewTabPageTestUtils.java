// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.test.transit.Condition.whether;
import static org.chromium.base.test.transit.Triggers.noopTo;

import android.app.Activity;
import android.content.Context;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.transit.SimpleConditions;
import org.chromium.base.test.transit.TripBuilder;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.omnibox.SearchEngineService;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.tile.TileSectionType;
import org.chromium.chrome.browser.suggestions.tile.TileSource;
import org.chromium.chrome.browser.suggestions.tile.TileTitleSource;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.url_constants.UrlOverrideUtils;
import org.chromium.chrome.test.transit.ntp.IncognitoNewTabPageStation;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.CtaPageStation;
import org.chromium.components.search_engines.AiModeButtonUiConfig;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/** Utilities for testing the NTP. */
public class NewTabPageTestUtils {
    private static final String TEST_PAGE = "/chrome/test/data/android/navigate/simple.html";

    /**
     * Waits for the NTP owned by the passed in tab to be fully loaded.
     *
     * <p>Hops on Public Transit at the {@link RegularNewTabPageStation} or {@link
     * IncognitoNewTabPageStation} in |tab|'s Activity. The returned Station is left ACTIVE; callers
     * that are not using Public Transit can ignore it, and the next hop on will discard it.
     *
     * <p>|tab| must be its Activity's current tab, since the NTP Stations check the Activity's
     * current tab.
     *
     * <p>A Public Transit test that holds a live Station should travel to the NTP Station from it
     * instead; calling this util from one logs a warning and makes the caller's Station unusable.
     *
     * @param tab The tab to be monitored for NTP loading.
     * @return the NTP Station hopped on at.
     */
    public static CtaPageStation waitForNtpLoaded(final Tab tab) {
        ChromeTabbedActivity activity =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            WindowAndroid windowAndroid = tab.getWindowAndroid();
                            assertNotNull("Tab is not attached to a window", windowAndroid);
                            Activity tabActivity = windowAndroid.getActivity().get();
                            assertTrue(
                                    "Tab is not in a ChromeTabbedActivity: " + tabActivity,
                                    tabActivity instanceof ChromeTabbedActivity);
                            ChromeTabbedActivity cta = (ChromeTabbedActivity) tabActivity;
                            // The NTP Stations follow the Activity's current tab, not |tab|.
                            assertEquals(
                                    "Tab is not the Activity's current tab",
                                    tab,
                                    cta.getActivityTab());
                            return cta;
                        });

        if (tab.isIncognito()) {
            return noopTo().hopOnTo(
                            activity,
                            IncognitoNewTabPageStation.newBuilder()
                                    .withTabAlreadySelected(tab)
                                    .build());
        }

        TripBuilder trip = noopTo();
        if (UrlOverrideUtils.isWebUiNtpOverrideEnabled()) {
            // With the WebUI NTP override, RegularNewTabPageStation skips the native page
            // Conditions, so keep waiting for the renderer to be ready.
            trip =
                    trip.waitForAnd(
                            SimpleConditions.uiThreadCondition(
                                    "WebUI NTP renderer is ready",
                                    () ->
                                            whether(
                                                    ChromeTabUtils.isRendererReady(tab),
                                                    "isRendererReady")));
        }
        return trip.hopOnTo(
                activity,
                RegularNewTabPageStation.newBuilder().withTabAlreadySelected(tab).build());
    }

    public static List<SiteSuggestion> createFakeSiteSuggestions() {
        return createFakeSiteSuggestions("http://127.0.0.1" + TEST_PAGE);
    }

    public static List<SiteSuggestion> createFakeSiteSuggestions(EmbeddedTestServer testServer) {
        return createFakeSiteSuggestions(testServer.getURL(TEST_PAGE));
    }

    public static List<SiteSuggestion> createFakeSiteSuggestions(String baseUrl) {
        List<SiteSuggestion> siteSuggestions = new ArrayList<>();
        siteSuggestions.add(
                new SiteSuggestion(
                        "0 TOP_SITES",
                        new GURL(baseUrl + "#0"),
                        TileTitleSource.TITLE_TAG,
                        TileSource.TOP_SITES,
                        TileSectionType.PERSONALIZED));
        siteSuggestions.add(
                new SiteSuggestion(
                        "1 ALLOWLIST",
                        new GURL(baseUrl + "#1"),
                        TileTitleSource.UNKNOWN,
                        TileSource.ALLOWLIST,
                        TileSectionType.PERSONALIZED));
        siteSuggestions.add(
                new SiteSuggestion(
                        "2 TOP_SITES",
                        new GURL(baseUrl + "#2"),
                        TileTitleSource.TITLE_TAG,
                        TileSource.TOP_SITES,
                        TileSectionType.PERSONALIZED));
        siteSuggestions.add(
                new SiteSuggestion(
                        "3 TOP_SITES",
                        new GURL(baseUrl + "#3"),
                        TileTitleSource.TITLE_TAG,
                        TileSource.TOP_SITES,
                        TileSectionType.PERSONALIZED));
        siteSuggestions.add(
                new SiteSuggestion(
                        "4 TOP_SITES",
                        new GURL(baseUrl + "#4"),
                        TileTitleSource.TITLE_TAG,
                        TileSource.TOP_SITES,
                        TileSectionType.PERSONALIZED));
        siteSuggestions.add(
                new SiteSuggestion(
                        "5 TOP_SITES",
                        new GURL(baseUrl + "#5"),
                        TileTitleSource.TITLE_TAG,
                        TileSource.TOP_SITES,
                        TileSectionType.PERSONALIZED));
        siteSuggestions.add(
                new SiteSuggestion(
                        "6 TOP_SITES",
                        new GURL(baseUrl + "#6"),
                        TileTitleSource.TITLE_TAG,
                        TileSource.TOP_SITES,
                        TileSectionType.PERSONALIZED));
        siteSuggestions.add(
                new SiteSuggestion(
                        "7 TOP_SITES",
                        new GURL(baseUrl + "#7"),
                        TileTitleSource.TITLE_TAG,
                        TileSource.TOP_SITES,
                        TileSectionType.PERSONALIZED));
        return siteSuggestions;
    }

    /**
     * Fakes the {@link AiModeButtonUiConfig} that Google offers, so that the AI Mode button is
     * shown on the NTP. Native only hands a config to a client which is eligible for AI Mode, which
     * a test device isn't, so without this the entry point stays hidden.
     */
    public static void fakeGoogleAiModeButtonUiConfig() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Context context = ContextUtils.getApplicationContext();
                    // Google's entry point is rendered from built-in assets and navigated to via
                    // the regular search engine plumbing, so its URLs are empty.
                    AiModeButtonUiConfig config =
                            new AiModeButtonUiConfig(
                                    context.getString(R.string.ai_mode_entrypoint_label),
                                    "Ask AI Mode in Google Search",
                                    "AI Mode button, press Enter to ask AI Mode",
                                    "Always show AI Mode",
                                    "Press tab then enter to ask AI Mode",
                                    /* faviconUrl= */ GURL.emptyGURL(),
                                    /* navigationUrl= */ "",
                                    /* navigationUrlEmpty= */ GURL.emptyGURL());
                    SearchEngineService.getForProfile(ProfileManager.getLastUsedRegularProfile())
                            .setAiModeButtonUiConfigForTesting(config);
                });
    }
}
