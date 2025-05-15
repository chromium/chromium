// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import static org.junit.Assert.assertTrue;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.Token;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.Transition.Trigger;
import org.chromium.base.test.transit.TravelException;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.test.transit.hub.NewTabGroupDialogFacility;
import org.chromium.chrome.test.transit.hub.TabSwitcherGroupCardFacility;
import org.chromium.chrome.test.transit.hub.TabSwitcherListEditorFacility;
import org.chromium.chrome.test.transit.hub.TabSwitcherStation;
import org.chromium.chrome.test.transit.page.PageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.transit.tabmodel.TabThumbnailCondition;
import org.chromium.chrome.test.util.TabBinningUtil;
import org.chromium.chrome.test.util.tabmodel.TabBinList;
import org.chromium.chrome.test.util.tabmodel.TabBinList.TabBinPosition;

import java.util.ArrayList;
import java.util.List;
import java.util.Objects;

/* Helper class for extended multi-stage Trips. */
public class Journeys {
    public static final String TAG = "Journeys";

    /**
     * Make Chrome have {@code numRegularTabs} of regular Tabs and {@code numIncognitoTabs} of
     * incognito tabs with {@code url} loaded.
     *
     * @param <T> specific type of PageStation for all opened tabs.
     * @param startingStation The current active station.
     * @param numRegularTabs The number of regular tabs.
     * @param numIncognitoTabs The number of incognito tabs.
     * @param url The URL to load.
     * @param pageStationFactory A factory method to create the PageStations for each tab.
     * @return the last opened tab's PageStation.
     */
    public static <T extends PageStation> T prepareTabs(
            PageStation startingStation,
            int numRegularTabs,
            int numIncognitoTabs,
            String url,
            Supplier<PageStation.Builder<T>> pageStationFactory) {
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
     * Same as {@link #prepareTabs(PageStation, int, int, String, Supplier)}, but ensures tab
     * thumbnails are captured to disk.
     */
    public static <T extends PageStation> T prepareTabsWithThumbnails(
            PageStation startingStation,
            int numRegularTabs,
            int numIncognitoTabs,
            String url,
            Supplier<PageStation.Builder<T>> pageStationFactory) {
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
                /* captureThumbnails= */ false);
    }

    /**
     * Create {@code numTabs} of {@link Tab}s with {@code url} loaded to Chrome.
     *
     * @param <T> specific type of PageStation for all opened tabs.
     * @param startingPage The current active station.
     * @param urls The URLs to load.
     * @param isIncognito Whether to open an incognito tab.
     * @param pageStationFactory A factory method to create the PageStations for each tab.
     * @return the last opened tab's PageStation.
     */
    @SuppressWarnings("unused")
    private static <T extends PageStation> T createTabs(
            final PageStation startingPage,
            List<String> urls,
            boolean isIncognito,
            Supplier<PageStation.Builder<T>> pageStationFactory) {
        return doCreateTabs(
                startingPage,
                urls,
                isIncognito,
                pageStationFactory,
                /* captureThumbnails= */ false);
    }

    /** Creates identical tabs and ensures tab thumbnails are captured to disk. */
    public static <T extends PageStation> T createTabsWithThumbnails(
            final PageStation startingPage,
            int numTabs,
            String url,
            boolean isIncognito,
            Supplier<PageStation.Builder<T>> pageStationFactory) {
        List<String> urls = getListOfIdenticalUrls(numTabs, url);
        return doCreateTabs(
                startingPage, urls, isIncognito, pageStationFactory, /* captureThumbnails= */ true);
    }

    /** Open and display multiple web pages in regular tabs, return the last page. */
    public static WebPageStation createRegularTabsWithWebPages(
            final PageStation startingPage, List<String> urls) {
        return doCreateTabs(
                startingPage,
                urls,
                /* isIncognito= */ false,
                WebPageStation::newBuilder,
                /* captureThumbnails= */ false);
    }

    /** Open and display multiple web pages in incognito tabs, return the last page. */
    public static WebPageStation createIncognitoTabsWithWebPages(
            final PageStation startingPage, List<String> urls) {
        return doCreateTabs(
                startingPage,
                urls,
                /* isIncognito= */ true,
                () -> WebPageStation.newBuilder().withIncognito(true),
                /* captureThumbnails= */ false);
    }

    // TODO(crbug.com/411430975): Open all tabs at once instead of one by one.
    private static <T extends PageStation> T doPrepareTabs(
            PageStation startingStation,
            List<String> urlsForRegularTabs,
            List<String> urlsForIncognitoTabs,
            Supplier<PageStation.Builder<T>> pageStationFactory,
            boolean captureThumbnails) {
        assert urlsForRegularTabs.size() >= 1;
        TabModelSelector tabModelSelector =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> startingStation.getActivity().getTabModelSelector());
        int currentTabCount = tabModelSelector.getModel(/* incognito= */ false).getCount();
        int currentIncognitoTabCount = tabModelSelector.getModel(/* incognito= */ true).getCount();
        assert currentTabCount == 1;
        assert currentIncognitoTabCount == 0;
        T station =
                startingStation.loadPageProgrammatically(
                        urlsForRegularTabs.get(0), pageStationFactory.get());
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
            station =
                    doCreateTabs(
                            station,
                            urlsForIncognitoTabs,
                            /* isIncognito= */ true,
                            pageStationFactory,
                            captureThumbnails);
        }
        return station;
    }

    private static <T extends PageStation> T doCreateTabs(
            final PageStation startingPage,
            List<String> urls,
            boolean isIncognito,
            Supplier<PageStation.Builder<T>> pageStationFactory,
            boolean captureThumbnails) {
        assert !urls.isEmpty();

        TabModelSelector tabModelSelector = startingPage.getActivity().getTabModelSelector();

        PageStation currentPage = startingPage;
        for (int i = 0; i < urls.size(); i++) {
            String url = urls.get(i);
            PageStation previousPage = currentPage;
            Tab previousTab = previousPage.loadedTabElement.get();
            if (i == 0 && startingPage.isIncognito() && !isIncognito) {
                currentPage =
                        currentPage
                                .openNewTabFast()
                                .loadPageProgrammatically(url, pageStationFactory.get());
            } else if (i == 0 && !startingPage.isIncognito() && isIncognito) {
                currentPage =
                        currentPage
                                .openNewIncognitoTabFast()
                                .loadPageProgrammatically(url, pageStationFactory.get());
            } else {
                currentPage = currentPage.openFakeLink(url, pageStationFactory.get());
            }

            if (!captureThumbnails) {
                continue;
            }

            boolean tryToFixThumbnail = false;
            try {
                Condition.waitFor(
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

                Tab tabToComeBackTo = currentPage.loadedTabElement.get();
                PageStation previousPageAgain =
                        currentPage.selectTabFast(previousTab, PageStation::newGenericBuilder);
                currentPage = previousPageAgain.selectTabFast(tabToComeBackTo, pageStationFactory);

                Condition.waitFor(
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
        TabModel tabModel = tabSwitcher.tabModelSelectorElement.get().getCurrentModel();
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
        TabModel currentModel = tabSwitcher.tabModelSelectorElement.get().getCurrentModel();
        TabSwitcherListEditorFacility editor = tabSwitcher.openAppMenu().clickSelectTabs();

        TabBinList tabBinList = TabBinningUtil.binTabsByCard(currentModel);
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
     * @param station the station to begin the flow from.
     * @param trigger The trigger used to begin the flow.
     */
    public static <HostStationT extends Station<ChromeTabbedActivity>>
            NewTabGroupDialogFacility<HostStationT> beginNewTabGroupUiFlow(
                    HostStationT station, Trigger trigger) {
        assertTrue(ChromeFeatureList.sTabGroupEntryPointsAndroid.isEnabled());

        SoftKeyboardFacility softKeyboard = new SoftKeyboardFacility();
        NewTabGroupDialogFacility<HostStationT> dialog =
                new NewTabGroupDialogFacility<>(softKeyboard);
        station.enterFacilitiesSync(List.of(dialog, softKeyboard), trigger);
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

    private static List<String> getListOfIdenticalUrls(int n, String url) {
        List<String> regularTabs = new ArrayList<>();
        for (int i = 0; i < n; i++) {
            regularTabs.add(url);
        }
        return regularTabs;
    }
}
