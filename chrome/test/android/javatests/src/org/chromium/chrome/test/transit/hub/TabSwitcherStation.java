// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.hub;

import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.instanceOf;
import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.chrome.test.util.ChromeTabUtils.getTabCountOnUiThread;

import android.view.View;

import androidx.recyclerview.widget.RecyclerView;

import com.google.errorprone.annotations.CheckReturnValue;

import org.hamcrest.Matcher;

import org.chromium.base.test.transit.TripBuilder;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.base.test.transit.ViewSpec;
import org.chromium.base.test.util.ViewActionOnDescendant;
import org.chromium.chrome.browser.hub.HubUtils;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabGridView;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.SoftKeyboardFacility;
import org.chromium.chrome.test.transit.page.BasePageStation;
import org.chromium.chrome.test.transit.page.CtaPageStation;
import org.chromium.chrome.test.transit.tabmodel.TabCountChangedCondition;
import org.chromium.chrome.test.util.TabBinningUtil;

import java.util.List;

/** The base station for Hub tab switcher stations. */
public abstract class TabSwitcherStation extends HubBaseStation {
    public static final Matcher<View> TAB_CLOSE_BUTTON =
            allOf(
                    withId(R.id.action_button),
                    isDescendantOfA(
                            allOf(
                                    withId(R.id.content_view),
                                    withParent(instanceOf(TabGridView.class)))),
                    isDisplayed());
    public static final Matcher<View> TAB_THUMBNAIL =
            allOf(
                    withId(R.id.tab_thumbnail),
                    isDescendantOfA(
                            allOf(
                                    withId(R.id.content_view),
                                    withParent(instanceOf(TabGridView.class)))),
                    isDisplayed());

    public ViewElement<RecyclerView> recyclerViewElement;
    public ViewElement<View> searchElement;
    public ViewElement<View> newTabButtonElement;

    public TabSwitcherStation(
            boolean isIncognito, boolean regularTabsExist, boolean incognitoTabsExist) {
        super(isIncognito, regularTabsExist, incognitoTabsExist, /* hasMenuButton= */ true);

        newTabButtonElement =
                declareView(toolbarElement.descendant(withId(R.id.toolbar_action_button)));
        declareElementFactory(
                mActivityElement,
                delayedElements -> {
                    Matcher<View> searchBox = withId(R.id.search_box);
                    ViewSpec<View> searchLoupe =
                            toolbarElement.descendant(withId(R.id.search_loupe));
                    if (shouldHubSearchBoxBeVisible()) {
                        searchElement = delayedElements.declareView(searchLoupe);
                        delayedElements.declareNoView(searchBox);
                    } else {
                        searchElement = delayedElements.declareView(searchBox);
                        delayedElements.declareNoView(searchLoupe);
                    }
                });
    }

    /**
     * Opens the app menu.
     *
     * @return the {@link TabSwitcherAppMenuFacility} for the Hub.
     */
    public TabSwitcherAppMenuFacility openAppMenu() {
        recheckActiveConditions();

        return menuButtonElement
                .clickTo()
                .enterFacility(new TabSwitcherAppMenuFacility<>(mIsIncognito));
    }

    /**
     * @param index The tab index to select.
     * @param destinationBuilder Builder for the specific type of {@link CtaPageStation} expected to
     *     appear.
     * @return Builder of the {@link CtaPageStation} for the tab that was selected.
     */
    public <T extends CtaPageStation> T selectTabAtIndex(
            int index, BasePageStation.Builder<T> destinationBuilder) {
        recheckActiveConditions();

        return selectTabAtCardIndexTo(index)
                .arriveAt(
                        destinationBuilder
                                .withIncognito(mIsIncognito)
                                .initSelectingExistingTab()
                                .build());
    }

    /** Click the thumbnail of the card at the given |index| to start a Trip. */
    @CheckReturnValue
    public TripBuilder selectTabAtCardIndexTo(int index) {
        return runTo(
                () ->
                        ViewActionOnDescendant.performOnRecyclerViewNthItemDescendant(
                                is(recyclerViewElement.value()), index, TAB_THUMBNAIL, click()));
    }

    /**
     * Close a tab and end in a destination.
     *
     * @param index The index of the tab to close.
     */
    public <T extends TabSwitcherStation> T closeTabAtIndex(
            int index, Class<T> expectedDestination) {
        TabModelSelector tabModelSelector = tabModelSelectorElement.value();
        boolean incognitoModelSelected = tabModelSelector.isOffTheRecordModelSelected();
        int expectedIncognitoTabs =
                getTabCountOnUiThread(tabModelSelector.getModel(/* incognito= */ true));
        int expectedRegularTabs =
                getTabCountOnUiThread(tabModelSelector.getModel(/* incognito= */ false));

        // By default stay in the same tab switcher state, unless closing the last incognito tab.
        boolean landInIncognitoSwitcher = false;
        if (getPaneId() == PaneId.INCOGNITO_TAB_SWITCHER) {
            assertTrue(incognitoModelSelected);
            expectedIncognitoTabs--;
            if (getTabCountOnUiThread(tabModelSelector.getCurrentModel()) <= 1) {
                landInIncognitoSwitcher = false;
            } else {
                landInIncognitoSwitcher = true;
            }
        } else {
            expectedRegularTabs--;
        }

        T tabSwitcher =
                expectedDestination.cast(
                        HubStationUtils.createHubStation(
                                landInIncognitoSwitcher
                                        ? PaneId.INCOGNITO_TAB_SWITCHER
                                        : PaneId.TAB_SWITCHER,
                                expectedRegularTabs > 0,
                                expectedIncognitoTabs > 0));
        return clickCloseTabOnCardIndexTo(index)
                .waitForAnd(
                        new TabCountChangedCondition(
                                tabModelSelector.getModel(incognitoModelSelected),
                                /* expectedChange= */ -1))
                .arriveAt(tabSwitcher);
    }

    /** Click the close tab button on the tab card at the given |index| to start a Trip. */
    @CheckReturnValue
    public TripBuilder clickCloseTabOnCardIndexTo(int index) {
        return runTo(
                () ->
                        ViewActionOnDescendant.performOnRecyclerViewNthItemDescendant(
                                is(recyclerViewElement.value()), index, TAB_CLOSE_BUTTON, click()));
    }

    /**
     * Returns to the previous tab via the back button.
     *
     * @param destinationBuilder Builder for the specific type of {@link CtaPageStation} expected to
     *     appear.
     * @return the {@link CtaPageStation} that Hub returned to.
     */
    public <T extends CtaPageStation> T leaveHubToPreviousTabViaBack(
            BasePageStation.Builder<T> destinationBuilder) {
        T destination =
                destinationBuilder.initSelectingExistingTab().withIncognito(mIsIncognito).build();
        return pressBackTo().withRetry().arriveAt(destination);
    }

    /** Expect a tab group card to exist. */
    public TabSwitcherGroupCardFacility expectGroupCard(List<Integer> tabIdsInGroup, String title) {
        TabModel currentModel = tabModelElement.value();
        int expectedCardIndex =
                runOnUiThreadBlocking(
                        () -> TabBinningUtil.getBinIndex(currentModel, tabIdsInGroup));
        return noopTo().enterFacility(
                        new TabSwitcherGroupCardFacility(expectedCardIndex, tabIdsInGroup, title));
    }

    /** Expect a tab card to exist. */
    public TabSwitcherTabCardFacility expectTabCard(int tabId, String title) {
        TabModel currentModel = tabModelElement.value();
        int expectedCardIndex =
                runOnUiThreadBlocking(() -> TabBinningUtil.getBinIndex(currentModel, tabId));
        return noopTo().enterFacility(
                        new TabSwitcherTabCardFacility(expectedCardIndex, tabId, title));
    }

    /** Verify the tab switcher card count. */
    public void verifyTabSwitcherCardCount(int count) {
        assertEquals(recyclerViewElement.value().getChildCount(), count);
    }

    public TabSwitcherSearchStation openTabSwitcherSearch() {
        TabSwitcherSearchStation searchStation = new TabSwitcherSearchStation(mIsIncognito);
        SoftKeyboardFacility softKeyboard = new SoftKeyboardFacility();
        searchElement.clickTo().arriveAt(searchStation, softKeyboard);
        softKeyboard.close();
        return searchStation;
    }

    private boolean shouldHubSearchBoxBeVisible() {
        return HubUtils.isScreenWidthTablet(
                mActivityElement.value().getResources().getConfiguration().screenWidthDp);
    }
}
