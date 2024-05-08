// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.CoreMatchers.allOf;

import static org.chromium.base.test.transit.ViewElement.sharedViewElement;

import android.view.View;

import androidx.annotation.CallSuper;
import androidx.annotation.IdRes;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.hamcrest.TypeSafeMatcher;

import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.ScrollableFacility;
import org.chromium.base.test.transit.Station;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ui.appmenu.AppMenuItemProperties;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;

import java.util.concurrent.Callable;

/**
 * Base class for app menus shown when pressing ("...").
 *
 * @param <HostStationT> the type of host {@link Station} where this app menu is opened.
 */
public abstract class AppMenuFacility<HostStationT extends Station>
        extends ScrollableFacility<HostStationT> {

    /** Create a new app menu item stub which throws UnsupportedOperationException if selected. */
    public Item<Void> newStubMenuItem(@IdRes int id) {
        return super.newStubItem(itemViewMatcher(id), itemDataMatcher(id));
    }

    /** Create a new app menu item which runs |selectHandler| when selected. */
    public <SelectReturnT> Item<SelectReturnT> newMenuItem(
            @IdRes int id, Callable<SelectReturnT> selectHandler) {
        return super.newItem(itemViewMatcher(id), itemDataMatcher(id), selectHandler);
    }

    /** Create a new app menu item which transitions to a |DestinationStationT| when selected. */
    public <DestinationStationT extends Station> Item<DestinationStationT> newMenuItemToStation(
            @IdRes int id, Callable<DestinationStationT> destinationStationFactory) {
        return super.newItemToStation(
                itemViewMatcher(id), itemDataMatcher(id), destinationStationFactory);
    }

    /** Create a new app menu item which enters a |EnteredFacilityT| when selected. */
    public <EnteredFacilityT extends Facility<HostStationT>>
            Item<EnteredFacilityT> newMenuItemToFacility(
                    @IdRes int id, Callable<EnteredFacilityT> destinationFacilityFactory) {
        return super.newItemToFacility(
                itemViewMatcher(id), itemDataMatcher(id), destinationFacilityFactory);
    }

    /** Create a new disabled app menu item. */
    public Item<Void> newDisabledMenuItem(@IdRes int id) {
        return super.newDisabledItem(itemViewMatcher(id), itemDataMatcher(id));
    }

    /** Create a new app menu item expected to be absent. */
    public Item<Void> newAbsentMenuItem(@IdRes int id) {
        return super.newAbsentItem(itemViewMatcher(id), itemDataMatcher(id));
    }

    public static final Matcher<View> MENU_LIST = withId(R.id.app_menu_list);
    public static final @IdRes int NEW_TAB_ID = R.id.new_tab_menu_id;
    public static final @IdRes int NEW_INCOGNITO_TAB_ID = R.id.new_incognito_tab_menu_id;
    public static final @IdRes int SETTINGS_ID = R.id.preferences_id;
    protected final ChromeTabbedActivityTestRule mChromeTabbedActivityTestRule;

    protected AppMenuFacility(
            HostStationT station, ChromeTabbedActivityTestRule chromeTabbedActivityTestRule) {
        super(station);
        mChromeTabbedActivityTestRule = chromeTabbedActivityTestRule;
    }

    @CallSuper
    @Override
    public void declareElements(Elements.Builder elements) {
        elements.declareView(sharedViewElement(MENU_LIST));

        super.declareElements(elements);
    }

    @Override
    public int getMinimumOnScreenItemCount() {
        // Expect at least the first two menu items, it's enough to establish the transition is
        // done.
        return 2;
    }

    /** Default behavior for "Open new tab". */
    protected NewTabPageStation createNewTabPageStation() {
        return NewTabPageStation.newBuilder()
                .withActivityTestRule(mChromeTabbedActivityTestRule)
                .withIsOpeningTabs(1)
                .withIsSelectingTabs(1)
                .build();
    }

    /** Default behavior for "Open new Incognito tab". */
    protected IncognitoNewTabPageStation createIncognitoNewTabPageStation() {
        return IncognitoNewTabPageStation.newBuilder()
                .withActivityTestRule(mChromeTabbedActivityTestRule)
                .withIsOpeningTabs(1)
                .withIsSelectingTabs(1)
                .build();
    }

    /** Default behavior for "Settings". */
    protected SettingsStation createSettingsStation() {
        return new SettingsStation();
    }

    private static Matcher<View> itemViewMatcher(@IdRes int id) {
        return allOf(withId(id), isDescendantOfA(MENU_LIST));
    }

    private static Matcher<ListItem> itemDataMatcher(@IdRes int id) {
        return withMenuItemId(id);
    }

    private static Matcher<MVCListAdapter.ListItem> withMenuItemId(@IdRes int id) {
        return new TypeSafeMatcher<>() {
            @Override
            public void describeTo(Description description) {
                description.appendText("with menu item id ");
                description.appendText(String.valueOf(id));
            }

            @Override
            protected boolean matchesSafely(MVCListAdapter.ListItem listItem) {
                return listItem.model.get(AppMenuItemProperties.MENU_ITEM_ID) == id;
            }
        };
    }
}
