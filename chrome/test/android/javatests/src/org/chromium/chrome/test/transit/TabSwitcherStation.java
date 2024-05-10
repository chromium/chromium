// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.either;
import static org.hamcrest.CoreMatchers.instanceOf;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.test.transit.LogicalElement.uiThreadLogicalElement;
import static org.chromium.base.test.transit.ViewElement.sharedViewElement;

import android.view.View;

import androidx.annotation.CallSuper;

import org.hamcrest.Matcher;

import org.chromium.base.test.transit.ActivityElement;
import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.Trip;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.base.test.util.ViewActionOnDescendant;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.hub.HubFieldTrial;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabGridView;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ToolbarTestUtils;

/**
 * The tab switcher screen, with the tab grid and the tab management toolbar.
 *
 * <p>Instantiate one of its subclasses:
 *
 * <ul>
 *   <li>RegularTabSwitcherStation
 *   <li>IncognitoTabSwitcherStation
 * </ul>
 */
public abstract class TabSwitcherStation extends Station {
    public static final ViewElement TOOLBAR =
            sharedViewElement(withId(ToolbarTestUtils.TAB_SWITCHER_TOOLBAR));
    public static final ViewElement TOOLBAR_NEW_TAB_BUTTON =
            sharedViewElement(
                    either(withId(ToolbarTestUtils.TAB_SWITCHER_TOOLBAR_NEW_TAB))
                            .or(withId(ToolbarTestUtils.TAB_SWITCHER_TOOLBAR_NEW_TAB_VARIATION)));

    public static final ViewElement INCOGNITO_TOGGLE_TAB_BUTTON =
            sharedViewElement(
                    allOf(
                            withContentDescription(
                                    R.string.accessibility_tab_switcher_incognito_stack)));
    public static final ViewElement REGULAR_TOGGLE_TAB_BUTTON =
            sharedViewElement(
                    allOf(
                            withContentDescription(
                                    R.string.accessibility_tab_switcher_standard_stack)));

    public static final ViewElement INCOGNITO_TOGGLE_TABS =
            sharedViewElement(withId(R.id.incognito_toggle_tabs));
    public static final ViewElement RECYCLER_VIEW =
            sharedViewElement(
                    allOf(
                            withId(R.id.tab_list_recycler_view),
                            withParent(withId(R.id.compositor_view_holder))));
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

    protected final ChromeTabbedActivityTestRule mChromeTabbedActivityTestRule;
    protected final boolean mIsIncognito;
    protected ActivityElement<ChromeTabbedActivity> mActivityElement;
    protected TabModelSelectorCondition mTabModelSelectorCondition;

    /** Instantiate one of the subclasses instead. */
    protected TabSwitcherStation(
            ChromeTabbedActivityTestRule chromeTabbedActivityTestRule, boolean incognito) {
        super();

        assert !HubFieldTrial.isHubEnabled();
        mChromeTabbedActivityTestRule = chromeTabbedActivityTestRule;
        mIsIncognito = incognito;
    }

    @Override
    @CallSuper
    public void declareElements(Elements.Builder elements) {
        mActivityElement = elements.declareActivity(ChromeTabbedActivity.class);
        mTabModelSelectorCondition =
                elements.declareEnterCondition(new TabModelSelectorCondition(mActivityElement));

        elements.declareView(TOOLBAR);
        elements.declareView(TOOLBAR_NEW_TAB_BUTTON);

        elements.declareLogicalElement(
                uiThreadLogicalElement(
                        "LayoutManager is showing TAB_SWITCHER", this::isTabSwitcherLayoutShowing));
    }

    public PageStation openNewTab() {
        recheckActiveConditions();

        PageStation page =
                PageStation.newPageStationBuilder()
                        .withActivityTestRule(mChromeTabbedActivityTestRule)
                        .withIncognito(mIsIncognito)
                        .withIsOpeningTabs(1)
                        .withIsSelectingTabs(1)
                        .build();
        return Trip.travelSync(this, page, () -> TOOLBAR_NEW_TAB_BUTTON.perform(click()));
    }

    public <T extends TabSwitcherStation> T closeTabAtIndex(
            int index, Class<T> expectedDestinationType) {

        TabModelSelector tabModelSelector = mActivityElement.get().getTabModelSelector();

        // By default stay in the same tab switcher state, unless closing the last incognito tab.
        boolean landInIncognitoSwitcher = mIsIncognito;
        if (mIsIncognito) {
            assertTrue(tabModelSelector.isIncognitoSelected());
            if (tabModelSelector.getCurrentModel().getCount() <= 1) {
                landInIncognitoSwitcher = false;
            }
        }

        T tabSwitcher;
        if (landInIncognitoSwitcher) {
            tabSwitcher =
                    expectedDestinationType.cast(
                            new IncognitoTabSwitcherStation(mChromeTabbedActivityTestRule));
        } else {
            tabSwitcher =
                    expectedDestinationType.cast(
                            new RegularTabSwitcherStation(mChromeTabbedActivityTestRule));
        }

        return Trip.travelSync(
                this,
                tabSwitcher,
                () ->
                        ViewActionOnDescendant.performOnRecyclerViewNthItemDescendant(
                                RECYCLER_VIEW.getViewMatcher(), index, TAB_CLOSE_BUTTON, click()));
    }

    public PageStation selectTabAtIndex(int index) {
        PageStation page =
                PageStation.newPageStationBuilder()
                        .withActivityTestRule(mChromeTabbedActivityTestRule)
                        .withIncognito(mIsIncognito)
                        .withIsOpeningTabs(0)
                        .withIsSelectingTabs(1)
                        .build();

        return Trip.travelSync(
                this,
                page,
                () ->
                        ViewActionOnDescendant.performOnRecyclerViewNthItemDescendant(
                                RECYCLER_VIEW.getViewMatcher(), index, TAB_THUMBNAIL, click()));
    }

    private boolean isHubDisabled() {
        return !HubFieldTrial.isHubEnabled();
    }

    private boolean isTabSwitcherLayoutShowing() {
        ChromeTabbedActivity activity = mActivityElement.get();
        if (activity == null) {
            return false;
        }
        LayoutManager layoutManager = activity.getLayoutManager();
        // TODO: Use #isLayoutFinishedShowing(LayoutType.TAB_SWITCHER) once available.
        return layoutManager.isLayoutVisible(LayoutType.TAB_SWITCHER);
    }
}
