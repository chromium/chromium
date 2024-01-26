// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.either;
import static org.hamcrest.CoreMatchers.instanceOf;
import static org.junit.Assert.assertTrue;

import android.view.View;

import androidx.annotation.CallSuper;

import org.hamcrest.Matcher;

import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.TransitStation;
import org.chromium.base.test.transit.Trip;
import org.chromium.base.test.transit.UiThreadCondition;
import org.chromium.base.test.util.ViewActionOnDescendant;
import org.chromium.chrome.browser.hub.HubFieldTrial;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.ClosableTabGridView;
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
public abstract class TabSwitcherStation extends TransitStation {
    public static final Matcher<View> TOOLBAR = withId(ToolbarTestUtils.TAB_SWITCHER_TOOLBAR);
    public static final Matcher<View> TOOLBAR_NEW_TAB_BUTTON =
            either(withId(ToolbarTestUtils.TAB_SWITCHER_TOOLBAR_NEW_TAB))
                    .or(withId(ToolbarTestUtils.TAB_SWITCHER_TOOLBAR_NEW_TAB_VARIATION));

    public static final Matcher<View> INCOGNITO_TOGGLE_TAB_BUTTON =
            allOf(withContentDescription(R.string.accessibility_tab_switcher_incognito_stack));
    public static final Matcher<View> REGULAR_TOGGLE_TAB_BUTTON =
            allOf(withContentDescription(R.string.accessibility_tab_switcher_standard_stack));

    public static final Matcher<View> INCOGNITO_TOGGLE_TABS = withId(R.id.incognito_toggle_tabs);
    public static final Matcher<View> EMPTY_STATE_TEXT =
            withText(R.string.tabswitcher_no_tabs_empty_state);
    public static final Matcher<View> RECYCLER_VIEW =
            allOf(
                    withId(R.id.tab_list_recycler_view),
                    withParent(withId(R.id.compositor_view_holder)));
    public static final Matcher<View> TAB_CLOSE_BUTTON =
            allOf(
                    withId(R.id.action_button),
                    isDescendantOfA(
                            allOf(
                                    withId(R.id.content_view),
                                    withParent(instanceOf(ClosableTabGridView.class)))),
                    isDisplayed());
    public static final Matcher<View> TAB_THUMBNAIL =
            allOf(
                    withId(R.id.tab_thumbnail),
                    isDescendantOfA(
                            allOf(
                                    withId(R.id.content_view),
                                    withParent(instanceOf(ClosableTabGridView.class)))),
                    isDisplayed());

    protected final ChromeTabbedActivityTestRule mChromeTabbedActivityTestRule;
    protected final boolean mIsIncognito;

    /** Instantiate one of the subclasses instead. */
    protected TabSwitcherStation(
            ChromeTabbedActivityTestRule chromeTabbedActivityTestRule, boolean incognito) {
        super();
        mChromeTabbedActivityTestRule = chromeTabbedActivityTestRule;
        mIsIncognito = incognito;
    }

    @Override
    @CallSuper
    public void declareElements(Elements.Builder elements) {
        elements.declareUnownedView(TOOLBAR);
        elements.declareUnownedView(TOOLBAR_NEW_TAB_BUTTON);

        elements.declareEnterCondition(new HubIsDisabled());
        elements.declareEnterCondition(new TabSwitcherLayoutShowing());
    }

    public PageStation openNewTab() {
        recheckEnterConditions();

        PageStation page = new PageStation(mChromeTabbedActivityTestRule, false, true);
        return Trip.travelSync(
                this,
                page,
                (t) -> {
                    t.addCondition(new TabSwitcherLayoutNotShowing());
                    onView(allOf(isDisplayed(), TOOLBAR_NEW_TAB_BUTTON)).perform(click());
                });
    }

    public <T extends TabSwitcherStation> T closeTabAtIndex(
            int index, Class<T> expectedDestinationType) {

        TabModelSelector tabModelSelector =
                mChromeTabbedActivityTestRule.getActivity().getTabModelSelector();

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
                (t) ->
                        ViewActionOnDescendant.performOnRecyclerViewNthItemDescendant(
                                RECYCLER_VIEW, index, TAB_CLOSE_BUTTON, click()));
    }

    public BasePageStation selectTabAtIndex(int index) {
        BasePageStation page =
                new PageStation(
                        mChromeTabbedActivityTestRule, mIsIncognito, /* isOpeningTab= */ false);

        return Trip.travelSync(
                this,
                page,
                (t) ->
                        ViewActionOnDescendant.performOnRecyclerViewNthItemDescendant(
                                RECYCLER_VIEW, index, TAB_THUMBNAIL, click()));
    }

    private class HubIsDisabled extends UiThreadCondition {
        @Override
        public boolean check() {
            return !HubFieldTrial.isHubEnabled();
        }

        @Override
        public String buildDescription() {
            return "HubFieldTrial Hub is disabled";
        }
    }

    protected class TabSwitcherLayoutShowing extends UiThreadCondition {
        @Override
        public boolean check() {
            LayoutManager layoutManager =
                    mChromeTabbedActivityTestRule.getActivity().getLayoutManager();
            // TODO: Use #isLayoutFinishedShowing(LayoutType.TAB_SWITCHER) once available.
            return layoutManager.isLayoutVisible(LayoutType.TAB_SWITCHER);
        }

        @Override
        public String buildDescription() {
            return "LayoutManager is showing TAB_SWITCHER";
        }
    }

    protected class TabSwitcherLayoutNotShowing extends UiThreadCondition {
        @Override
        public boolean check() {
            LayoutManager layoutManager =
                    mChromeTabbedActivityTestRule.getActivity().getLayoutManager();
            // TODO: Use #isLayoutHidden(LayoutType.TAB_SWITCHER) once available.
            return !layoutManager.isLayoutVisible(LayoutType.TAB_SWITCHER);
        }

        @Override
        public String buildDescription() {
            return "LayoutManager is not showing TAB_SWITCHER";
        }
    }
}
