// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import androidx.test.platform.app.InstrumentationRegistry;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.TravelException;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.test.transit.page.PageStation;
import org.chromium.chrome.test.transit.tabmodel.TabThumbnailCondition;
import org.chromium.chrome.test.util.ChromeTabUtils;

/* Helper class for extended multi-stage Trips. */
public class Journeys {
    public static final String TAG = "Journeys";

    /**
     * Make Chrome have {@code numRegularTabs} of regular Tabs and {@code numIncognitoTabs} of
     * incognito tabs with {@code url} loaded.
     *
     * <p>Ensures tab thumbnails are captured to disk.
     *
     * @param <T> specific type of PageStation for all opened tabs.
     * @param startingStation The current active station.
     * @param numRegularTabs The number of regular tabs.
     * @param numIncognitoTabs The number of incognito tabs.
     * @param url The URL to load.
     * @param pageStationFactory A factory method to create the PageStations for each tab.
     * @return the last opened tab's PageStation.
     */
    public static <T extends PageStation> T prepareTabsWithThumbnails(
            PageStation startingStation,
            int numRegularTabs,
            int numIncognitoTabs,
            String url,
            Supplier<PageStation.Builder<T>> pageStationFactory) {
        assert numRegularTabs >= 1;
        assert url != null;
        TabModelSelector tabModelSelector =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> startingStation.getActivity().getTabModelSelector());
        int currentTabCount = tabModelSelector.getModel(/* incognito= */ false).getCount();
        int currentIncognitoTabCount = tabModelSelector.getModel(/* incognito= */ true).getCount();
        assert currentTabCount == 1;
        assert currentIncognitoTabCount == 0;
        T station = startingStation.loadPageProgrammatically(url, pageStationFactory.get());
        // One tab already exists.
        if (numRegularTabs > 1) {
            station =
                    createTabsWithThumbnails(
                            station,
                            numRegularTabs - 1,
                            url,
                            /* isIncognito= */ false,
                            pageStationFactory);
        }
        if (numIncognitoTabs > 0) {
            station =
                    createTabsWithThumbnails(
                            station,
                            numIncognitoTabs,
                            url,
                            /* isIncognito= */ true,
                            pageStationFactory);
        }
        return station;
    }

    /**
     * Create {@code numTabs} of {@link Tab}s with {@code url} loaded to Chrome.
     *
     * <p>Ensures tab thumbnails are captured to disk.
     *
     * @param <T> specific type of PageStation for all opened tabs.
     * @param startingPage The current active station.
     * @param numTabs The number of tabs to create.
     * @param url The URL to load.
     * @param isIncognito Whether to open an incognito tab.
     * @param pageStationFactory A factory method to create the PageStations for each tab.
     * @return the last opened tab's PageStation.
     */
    public static <T extends PageStation> T createTabsWithThumbnails(
            final PageStation startingPage,
            int numTabs,
            String url,
            boolean isIncognito,
            Supplier<PageStation.Builder<T>> pageStationFactory) {
        assert numTabs > 0;

        TabModelSelector tabModelSelector = startingPage.getActivity().getTabModelSelector();
        TabModel tabModel = tabModelSelector.getModel(isIncognito);

        PageStation currentPage = startingPage;
        for (int i = 0; i < numTabs; i++) {
            final ChromeTabbedActivity activity = currentPage.getActivity();
            PageStation previousPage = currentPage;
            Tab previousTab = previousPage.getLoadedTab();
            currentPage =
                    currentPage.travelToSync(
                            pageStationFactory
                                    .get()
                                    .withIsOpeningTabs(1)
                                    .withIsSelectingTabs(1)
                                    .withIncognito(isIncognito)
                                    .withExpectedUrlSubstring(url)
                                    .build(),
                            () -> {
                                ChromeTabUtils.fullyLoadUrlInNewTab(
                                        InstrumentationRegistry.getInstrumentation(),
                                        activity,
                                        url,
                                        isIncognito);
                            });
            boolean tryToFixThumbnail = false;
            try {
                Condition.runAndWaitFor(
                        null,
                        TabThumbnailCondition.etc1(tabModel, i),
                        TabThumbnailCondition.jpeg(tabModel, i));
            } catch (TravelException e) {
                tryToFixThumbnail = true;
            }

            if (tryToFixThumbnail) {
                Log.w(
                        TAG,
                        "Missing previous tab's thumbnail (index %d, id %d), try to fix by"
                                + " selecting it",
                        i,
                        previousTab.getId());

                Tab tabToComeBackTo = currentPage.getLoadedTab();
                T previousPageAgain =
                        currentPage.travelToSync(
                                pageStationFactory
                                        .get()
                                        .withIncognito(isIncognito)
                                        .withIsOpeningTabs(0)
                                        .withIsSelectingTabs(1)
                                        .build(),
                                () -> selectTab(tabModelSelector, previousTab));
                currentPage =
                        previousPageAgain.travelToSync(
                                pageStationFactory
                                        .get()
                                        .withIncognito(isIncognito)
                                        .withIsOpeningTabs(0)
                                        .withIsSelectingTabs(1)
                                        .build(),
                                () -> selectTab(tabModelSelector, tabToComeBackTo));

                Condition.runAndWaitFor(
                        null,
                        TabThumbnailCondition.etc1(tabModel, i),
                        TabThumbnailCondition.jpeg(tabModel, i));
            }
        }
        return (T) currentPage;
    }

    private static void selectTab(TabModelSelector tabModelSelector, Tab tab) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModelUtils.selectTabById(
                            tabModelSelector, tab.getId(), TabSelectionType.FROM_USER);
                });
    }
}
