// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import java.util.ArrayList;
import java.util.List;

import java.util.Objects;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.Token;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.TravelException;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.test.transit.hub.TabSwitcherGroupCardFacility;
import org.chromium.chrome.test.transit.hub.TabSwitcherListEditorFacility;
import org.chromium.chrome.test.transit.hub.TabSwitcherStation;
import org.chromium.chrome.test.transit.page.PageStation;
import org.chromium.chrome.test.transit.tabmodel.TabThumbnailCondition;

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

        PageStation currentPage = startingPage;
        for (int i = 0; i < numTabs; i++) {
            PageStation previousPage = currentPage;
            Tab previousTab = previousPage.getLoadedTab();
            currentPage =
                    isIncognito
                            ? currentPage.openNewIncognitoTabFast()
                            : currentPage.openNewTabFast();
            currentPage = currentPage.loadPageProgrammatically(url, pageStationFactory.get());
            boolean tryToFixThumbnail = false;
            try {
                Condition.runAndWaitFor(
                        null,
                        TabThumbnailCondition.etc1(tabModelSelector, previousTab),
                        TabThumbnailCondition.jpeg(tabModelSelector, previousTab));
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
                PageStation previousPageAgain =
                        currentPage.selectTabFast(previousTab, PageStation::newGenericBuilder);
                currentPage = previousPageAgain.selectTabFast(tabToComeBackTo, pageStationFactory);

                Condition.runAndWaitFor(
                        null,
                        TabThumbnailCondition.etc1(tabModelSelector, previousTab),
                        TabThumbnailCondition.jpeg(tabModelSelector, previousTab));
            }
        }
        return (T) currentPage;
    }

    /**
     * Merge all tabs in the current TabModel from a TabSwitcherStation into a single tab group.
     *
     * @param tabSwitcher the TabSwitcherStation we will be merging tabs for.
     * @return the facility representing the tab group card created as a result of merging tabs.
     */
    public static TabSwitcherGroupCardFacility mergeAllTabsToNewGroup(
            TabSwitcherStation tabSwitcher) {
        TabModel tabModel = tabSwitcher.getTabModelSelectorSupplier().get().getCurrentModel();
        List<Tab> tabs = TabModelUtils.convertTabListToListOfTabs(tabModel);
        return mergeTabsToNewGroup(tabSwitcher, tabs);
    }

    /**
     * Merge a list of tabs in the current TabModel from a TabSwitcherStation into a single tab
     * group.
     *
     * @param tabSwitcher the TabSwitcherStation we will be merging tabs for.
     * @param tabs a list of tabs to be merged.
     * @return the facility representing the tab group card created as a result of merging tabs.
     */
    public static TabSwitcherGroupCardFacility mergeTabsToNewGroup(
            TabSwitcherStation tabSwitcher, List<Tab> tabs) {
        assert !tabs.isEmpty();
        TabModel currentModel = tabSwitcher.getTabModelSelectorSupplier().get().getCurrentModel();
        TabSwitcherListEditorFacility editor = tabSwitcher.openAppMenu().clickSelectTabs();
        for (Tab tab : tabs) {
            int index = currentModel.indexOf(tab);
            int tabId = tab.getId();
            editor = editor.addTabToSelection(index, tabId);
        }

        TabSwitcherGroupCardFacility groupCard = editor.openAppMenuWithEditor()
                .groupTabs().pressDone();

        verifyTabGroupMergeSuccessful(tabs, currentModel);
        return groupCard;
    }

    /**
     * Verifies that the merger of several tabs into a tab group is correct.
     *
     * @param tabs the list of tabs that have been merged.
     * @param currentModel the TabModel containing the list of tabs that have been merged.
     */
    private static void verifyTabGroupMergeSuccessful(List<Tab> tabs, TabModel currentModel) {
        List<Token> tabGroupIdsOfGroupedTabs = new ArrayList<>();
        for (Tab tab : tabs) {
            int id = tab.getId();
            Tab tabById = currentModel.getTabById(id);
            if (tabById != null) {
                Token tabGroupId = tabById.getTabGroupId();
                tabGroupIdsOfGroupedTabs.add(tabGroupId);
            }
        }

        assert tabGroupIdsOfGroupedTabs.size() == tabs.size();

        // Assert all tokens are the same.
        Token baseToken = tabGroupIdsOfGroupedTabs.get(0);
        for (Token token : tabGroupIdsOfGroupedTabs) {
            assert Objects.equals(baseToken, token);
        }
    }
}
