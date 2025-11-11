// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import static org.junit.Assert.assertTrue;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.base.test.transit.Triggers.noopTo;
import static org.chromium.chrome.test.util.ChromeTabUtils.getTabCountOnUiThread;

import android.util.Pair;

import org.chromium.base.Log;
import org.chromium.base.Token;
import org.chromium.base.test.transit.TravelException;
import org.chromium.base.test.transit.TripBuilder;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.test.transit.hub.NewTabGroupDialogFacility;
import org.chromium.chrome.test.transit.hub.TabSwitcherGroupCardFacility;
import org.chromium.chrome.test.transit.hub.TabSwitcherListEditorFacility;
import org.chromium.chrome.test.transit.hub.TabSwitcherStation;
import org.chromium.chrome.test.transit.page.BasePageStation;
import org.chromium.chrome.test.transit.page.CtaPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.transit.tabmodel.TabThumbnailCondition;
import org.chromium.chrome.test.util.TabBinningUtil;
import org.chromium.chrome.test.util.tabmodel.TabBinList;
import org.chromium.chrome.test.util.tabmodel.TabBinList.TabBinPosition;

import java.util.ArrayList;
import java.util.List;
import java.util.Objects;
import java.util.function.Supplier;

/* Helper class for extended multi-stage Trips. */
public class Journeys {
    public static final String TAG = "Journeys";

    /**
     * Make Chrome have {@code numRegularTabs} of regular Tabs and {@code numIncognitoTabs} of
     * incognito tabs with {@code url} loaded. Incognito tabs are opened in the same window.
     *
     * @param <T> specific type of {@link CtaPageStation} for all opened tabs.
     * @param startingStation The current active station.
     * @param numRegularTabs The number of regular tabs.
     * @param numIncognitoTabs The number of incognito tabs.
     * @param url The URL to load.
     * @param pageStationFactory A factory method to create the PageStations for each tab.
     * @return the last opened tab's PageStation.
     */
    public static <T extends CtaPageStation> T prepareTabs(
            CtaPageStation startingStation,
            int numRegularTabs,
            int numIncognitoTabs,
            String url,
            Supplier<BasePageStation.Builder<T>> pageStationFactory) {
        List<String> regularTabs = getListOfIdenticalUrls(numRegularTabs, url);
        List<String> incognitoTabs = getListOfIdenticalUrls(numIncognitoTabs, url);

        Pair<T, T> stations =
                doPrepareTabs(
                        startingStation,
                        regularTabs,
                        incognitoTabs,
                        pageStationFactory,
                        /* captureThumbnails= */ false);
        return stations.second != null ? stations.second : stations.first;
    }

    /**
     * Make Chrome have {@code numRegularTabs} of regular Tabs and {@code numIncognitoTabs} of
     * incognito tabs with {@code url} loaded. Incognito tabs are opened in a separate window.
     *
     * @param <T> specific type of {@link CtaPageStation} for all opened tabs.
     * @param startingStation The current active station.
     * @param numRegularTabs The number of regular tabs.
     * @param numIncognitoTabs The number of incognito tabs.
     * @param url The URL to load.
     * @param pageStationFactory A factory method to create the PageStations for each tab.
     * @return A pair of the last opened regular and incognito tabs' PageStations.
     */
    public static <T extends CtaPageStation> Pair<T, T> prepareTabsSeparateWindows(
            CtaPageStation startingStation,
            int numRegularTabs,
            int numIncognitoTabs,
            String url,
            Supplier<BasePageStation.Builder<T>> pageStationFactory) {
        assertTrue(
                "This method should only be used when incognito tabs are opened in separate window",
                IncognitoUtils.shouldOpenIncognitoAsWindow());
        List<String> regularTabs = getListOfIdenticalUrls(numRegularTabs, url);
        List<String> incognitoTabs = getListOfIdenticalUrls(numIncognitoTabs, url);

        return doPrepareTabs(
                startingStation,
                regularTabs,
                incognitoTabs,
                pageStationFactory,
                /* captureThumbnails= */ false);
    }

    /**
     * Same as {@link #prepareTabs(CtaPageStation, int, int, String, Supplier)}, but ensures tab
     * thumbnails are captured to disk.
     */
    public static <T extends CtaPageStation> T prepareTabsWithThumbnails(
            CtaPageStation startingStation,
            int numRegularTabs,
            int numIncognitoTabs,
            String url,
            Supplier<BasePageStation.Builder<T>> pageStationFactory) {
        List<String> regularTabs = getListOfIdenticalUrls(numRegularTabs, url);
        List<String> incognitoTabs = getListOfIdenticalUrls(numIncognitoTabs, url);

        Pair<T, T> stations =
                doPrepareTabs(
                        startingStation,
                        regularTabs,
                        incognitoTabs,
                        pageStationFactory,
                        /* captureThumbnails= */ true);
        return stations.second != null ? stations.second : stations.first;
    }

    /**
     * Same as {@link #prepareTabsSeparateWindows(CtaPageStation, int, int, String, Supplier)}, but
     * ensures tab thumbnails are captured to disk.
     */
    public static <T extends CtaPageStation> Pair<T, T> prepareTabsWithThumbnailsSeparateWindows(
            CtaPageStation startingStation,
            int numRegularTabs,
            int numIncognitoTabs,
            String url,
            Supplier<BasePageStation.Builder<T>> pageStationFactory) {
        assertTrue(
                "This method should only be used when incognito tabs are opened in separate window",
                IncognitoUtils.shouldOpenIncognitoAsWindow());
        List<String> regularTabs = getListOfIdenticalUrls(numRegularTabs, url);
        List<String> incognitoTabs = getListOfIdenticalUrls(numIncognitoTabs, url);

        return doPrepareTabs(
                startingStation,
                regularTabs,
                incognitoTabs,
                pageStationFactory,
                /* captureThumbnails= */ true);
    }

    /**
     * Open and display multiple web pages in regular tabs, return the last page.
     *
     * <p>The first URL will be opened in the current active tab, the rest of the URLs will be
     * opened in new tabs.
     */
    public static WebPageStation prepareRegularTabsWithWebPages(
            WebPageStation webPageStation, List<String> urlsToOpen) {
        return doPrepareTabs(
                        webPageStation,
                        urlsToOpen,
                        List.of(),
                        WebPageStation::newBuilder,
                        /* captureThumbnails= */ false)
                .first;
    }

    /**
     * Create {@code numTabs} of {@link Tab}s with {@code url} loaded to Chrome.
     *
     * @param <T> specific type of {@link CtaPageStation} for all opened tabs.
     * @param startingPage The current active station.
     * @param urls The URLs to load.
     * @param isIncognito Whether to open an incognito tab.
     * @param pageStationFactory A factory method to create the PageStations for each tab.
     * @return the last opened tab's {@link CtaPageStation}.
     */
    @SuppressWarnings("unused")
    private static <T extends CtaPageStation> T createTabs(
            final CtaPageStation startingPage,
            List<String> urls,
            boolean isIncognito,
            Supplier<BasePageStation.Builder<T>> pageStationFactory) {
        return doCreateTabs(
                startingPage,
                urls,
                isIncognito,
                pageStationFactory,
                /* captureThumbnails= */ false);
    }

    /** Creates identical tabs and ensures tab thumbnails are captured to disk. */
    public static <T extends CtaPageStation> T createTabsWithThumbnails(
            final CtaPageStation startingPage,
            int numTabs,
            String url,
            boolean isIncognito,
            Supplier<BasePageStation.Builder<T>> pageStationFactory) {
        List<String> urls = getListOfIdenticalUrls(numTabs, url);
        return doCreateTabs(
                startingPage, urls, isIncognito, pageStationFactory, /* captureThumbnails= */ true);
    }

    /** Open and display multiple web pages in regular tabs, return the last page. */
    public static WebPageStation createRegularTabsWithWebPages(
            final CtaPageStation startingPage, List<String> urls) {
        return doCreateTabs(
                startingPage,
                urls,
                /* isIncognito= */ false,
                WebPageStation::newBuilder,
                /* captureThumbnails= */ false);
    }

    /** Open and display multiple web pages in incognito tabs, return the last page. */
    public static WebPageStation createIncognitoTabsWithWebPages(
            final CtaPageStation startingPage, List<String> urls) {
        return doCreateTabs(
                startingPage,
                urls,
                /* isIncognito= */ true,
                () -> WebPageStation.newBuilder().withIncognito(true),
                /* captureThumbnails= */ false);
    }

    // TODO(crbug.com/411430975): Open all tabs at once instead of one by one.
    private static <T extends CtaPageStation> Pair<T, T> doPrepareTabs(
            CtaPageStation startingStation,
            List<String> urlsForRegularTabs,
            List<String> urlsForIncognitoTabs,
            Supplier<BasePageStation.Builder<T>> pageStationFactory,
            boolean captureThumbnails) {
        assert urlsForRegularTabs.size() >= 1;
        TabModelSelector tabModelSelector = startingStation.getTabModelSelector();
        int currentTabCount =
                getTabCountOnUiThread(tabModelSelector.getModel(/* incognito= */ false));
        int currentIncognitoTabCount =
                getTabCountOnUiThread(tabModelSelector.getModel(/* incognito= */ true));
        assert currentTabCount == 1;
        assert currentIncognitoTabCount == 0;
        T station =
                startingStation.loadPageProgrammatically(
                        urlsForRegularTabs.get(0), pageStationFactory.get());
        T stationIncognito = null;
        // One tab already exists.
        if (urlsForRegularTabs.size() > 1) {
            var urlsForRegularTabsMinusFirst = new ArrayList<>(urlsForRegularTabs);
            urlsForRegularTabsMinusFirst.remove(0);
            station =
                    doCreateTabs(
                            station,
                            urlsForRegularTabsMinusFirst,
                            /* isIncognito= */ false,
                            pageStationFactory,
                            captureThumbnails);
        }
        if (urlsForIncognitoTabs.size() > 0) {
            stationIncognito =
                    doCreateTabs(
                            station,
                            urlsForIncognitoTabs,
                            /* isIncognito= */ true,
                            pageStationFactory,
                            captureThumbnails);
        }
        return new Pair<>(station, stationIncognito);
    }

    private static <T extends CtaPageStation> T doCreateTabs(
            final CtaPageStation startingPage,
            List<String> urls,
            boolean isIncognito,
            Supplier<BasePageStation.Builder<T>> pageStationFactory,
            boolean captureThumbnails) {
        assert !urls.isEmpty();

        TabModelSelector tabModelSelector = startingPage.getTabModelSelector();

        CtaPageStation currentPage = startingPage;
        for (int i = 0; i < urls.size(); i++) {
            String url = urls.get(i);
            CtaPageStation previousPage = currentPage;
            Tab previousTab = previousPage.loadedTabElement.value();
            if (i == 0 && startingPage.isIncognito() && !isIncognito) {
                currentPage =
                        currentPage
                                .openNewTabFast()
                                .loadPageProgrammatically(url, pageStationFactory.get());
            } else if (i == 0 && !startingPage.isIncognito() && isIncognito) {
                currentPage =
                        currentPage
                                .openNewIncognitoTabOrWindowFast()
                                .loadPageProgrammatically(url, pageStationFactory.get());
            } else {
                currentPage = currentPage.openFakeLink(url, pageStationFactory.get());
            }

            if (!captureThumbnails) {
                continue;
            }

            boolean tryToFixThumbnail = false;
            try {
                noopTo().waitFor(
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

                Tab tabToComeBackTo = currentPage.loadedTabElement.value();
                CtaPageStation previousPageAgain =
                        currentPage.selectTabFast(previousTab, CtaPageStation::newGenericBuilder);
                currentPage = previousPageAgain.selectTabFast(tabToComeBackTo, pageStationFactory);

                noopTo().waitFor(
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
        TabModel tabModel = tabSwitcher.tabModelElement.value();
        List<Tab> tabs =
                runOnUiThreadBlocking(() -> TabModelUtils.convertTabListToListOfTabs(tabModel));
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
        TabModel currentModel = tabSwitcher.tabModelElement.value();
        TabSwitcherListEditorFacility editor = tabSwitcher.openAppMenu().clickSelectTabs();

        TabBinList tabBinList =
                runOnUiThreadBlocking(() -> TabBinningUtil.binTabsByCard(currentModel));
        for (Tab tab : tabs) {
            TabBinPosition tabPosition = tabBinList.tabIdToPositionMap.get(tab.getId());
            assert tabPosition != null;

            int tabCardIndex = tabPosition.cardIndexInTabSwitcher;
            int tabId = tab.getId();
            editor = editor.addTabToSelection(tabCardIndex, tabId);
        }

        TabSwitcherGroupCardFacility groupCard =
                editor.openAppMenuWithEditor().groupTabs().pressDone();

        verifyTabGroupMergeSuccessful(tabs, currentModel);
        return groupCard;
    }

    /**
     * Begins a new tab group creation UI flow. See {@link TabGroupCreationUiDelegate}
     *
     * @param <HostStationT> The type of station this is scoped to.
     * @param tripBuilder TripBuilder with the Trigger to begin the flow from.
     */
    public static <HostStationT extends ChromeActivityTabModelBoundStation<ChromeTabbedActivity>>
            NewTabGroupDialogFacility<HostStationT> beginNewTabGroupUiFlow(
                    TripBuilder tripBuilder) {
        assertTrue(ChromeFeatureList.sTabGroupEntryPointsAndroid.isEnabled());

        SoftKeyboardFacility softKeyboard = new SoftKeyboardFacility();
        NewTabGroupDialogFacility<HostStationT> dialog =
                new NewTabGroupDialogFacility<>(softKeyboard);
        tripBuilder.enterFacilities(dialog, softKeyboard);
        return dialog;
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
            Tab tabById = runOnUiThreadBlocking(() -> currentModel.getTabById(id));
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

    private static List<String> getListOfIdenticalUrls(int n, String url) {
        List<String> regularTabs = new ArrayList<>();
        for (int i = 0; i < n; i++) {
            regularTabs.add(url);
        }
        return regularTabs;
    }
}
