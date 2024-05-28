// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import androidx.test.platform.app.InstrumentationRegistry;

import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/* Helper class for extended multi-stage Trips. */
public class Journeys {
    /**
     * Make Chrome have {@code numTabs} of regular Tabs and {@code numIncognitoTabs} of incognito
     * tabs with {@code url} loaded.
     *
     * @param startingStation The current active station.
     * @param numTabs The number of regular tabs.
     * @param numIncognitoTabs The number of incognito tabs.
     * @param url The URL to load.
     * @return the last opened tab PageStation (regular if numIncognitoTabs = 0, otherwise
     *     incognito).
     */
    public static PageStation prepareTabs(
            PageStation startingStation, int numTabs, int numIncognitoTabs, String url) {
        assert numTabs >= 1;
        assert url != null;
        TabModelSelector tabModelSelector =
                TestThreadUtils.runOnUiThreadBlockingNoException(
                        () -> startingStation.getActivity().getTabModelSelector());
        int currentTabCount = tabModelSelector.getModel(/* incognito= */ false).getCount();
        int currentIncognitoTabCount = tabModelSelector.getModel(/* incognito= */ true).getCount();
        assert currentTabCount == 1;
        assert currentIncognitoTabCount == 0;
        Tab currentTab = startingStation.getActivity().getActivityTab();
        PageStation pageStation =
                PageStation.newPageStationBuilder()
                        .withIsOpeningTabs(0)
                        .withTabAlreadySelected(currentTab)
                        .build();
        PageStation station = startingStation.loadPageProgramatically(pageStation, url);
        // One tab already exists.
        station = createTabs(station, numTabs - 1, url, /* isIncognito= */ false);
        if (numIncognitoTabs > 0) {
            station = createTabs(station, numIncognitoTabs, url, /* isIncognito= */ true);
        }
        return station;
    }

    /**
     * Create {@code numTabs} of {@link Tab}s with {@code url} loaded to Chrome.
     *
     * @param startingStation The current active station.
     * @param numTabs The number of tabs to create.
     * @param url The URL to load.
     * @param isIncognito Whether to open an incognito tab.
     * @return the last opened tab PageStation (regular unless {@code isIncognito}).
     */
    public static PageStation createTabs(
            PageStation startingStation, int numTabs, String url, boolean isIncognito) {
        for (int i = 0; i < numTabs; i++) {
            final ChromeTabbedActivity activity = startingStation.getActivity();
            PageStation pageStation =
                    PageStation.newPageStationBuilder()
                            .withIsOpeningTabs(1)
                            .withIsSelectingTabs(1)
                            .withIncognito(isIncognito)
                            .build();
            startingStation =
                    startingStation.travelToSync(
                            pageStation,
                            () -> {
                                ChromeTabUtils.fullyLoadUrlInNewTab(
                                        InstrumentationRegistry.getInstrumentation(),
                                        activity,
                                        url,
                                        isIncognito);
                            });
        }
        return startingStation;
    }
}
