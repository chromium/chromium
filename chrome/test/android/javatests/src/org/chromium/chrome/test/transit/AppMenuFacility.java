// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.CoreMatchers.allOf;

import static org.chromium.base.test.transit.ViewElement.sharedViewElement;

import android.view.View;

import androidx.annotation.CallSuper;
import androidx.annotation.IdRes;

import org.hamcrest.Matcher;

import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.StationFacility;
import org.chromium.base.test.transit.TransitStation;
import org.chromium.base.test.transit.Trip;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.R;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;

/**
 * Base class for app menus shown when pressing ("...").
 *
 * @param <T> the type of TransitStation where this app menu is opened.
 */
public abstract class AppMenuFacility<T extends TransitStation> extends StationFacility<T> {

    public static final Matcher<View> MENU_LIST = withId(R.id.app_menu_list);
    public static final ViewElement NEW_TAB_ITEM = itemElement(R.id.new_tab_menu_id);
    public static final ViewElement NEW_INCOGNITO_TAB_ITEM =
            itemElement(R.id.new_incognito_tab_menu_id);
    public static final ViewElement SETTINGS_ITEM = itemElement(R.id.preferences_id);
    protected final ChromeTabbedActivityTestRule mChromeTabbedActivityTestRule;

    protected AppMenuFacility(
            T station, ChromeTabbedActivityTestRule chromeTabbedActivityTestRule) {
        super(station);
        mChromeTabbedActivityTestRule = chromeTabbedActivityTestRule;
    }

    /** A ViewElement for an enabled menu item. */
    protected static ViewElement itemElement(@IdRes int id) {
        return sharedViewElement(allOf(withId(id), isDescendantOfA(MENU_LIST)));
    }

    /** A ViewElement for a disabled menu item. */
    protected static ViewElement disabledItemElement(@IdRes int id) {
        return sharedViewElement(
                allOf(withId(id), isDescendantOfA(MENU_LIST)),
                ViewElement.newOptions().expectDisabled().build());
    }

    @CallSuper
    @Override
    public void declareElements(Elements.Builder elements) {
        elements.declareView(sharedViewElement(MENU_LIST));
    }

    /** Selects "New tab" from the app menu. */
    public NewTabPageStation openNewTab() {
        recheckActiveConditions();

        NewTabPageStation destination =
                NewTabPageStation.newBuilder()
                        .withActivityTestRule(mChromeTabbedActivityTestRule)
                        .withIsOpeningTab(true)
                        .withIsSelectingTab(true)
                        .build();

        return Trip.travelSync(mStation, destination, () -> NEW_TAB_ITEM.perform(click()));
    }

    /** Selects "New Incognito tab" from the app menu. */
    public IncognitoNewTabPageStation openNewIncognitoTab() {
        recheckActiveConditions();

        IncognitoNewTabPageStation destination =
                IncognitoNewTabPageStation.newBuilder()
                        .withActivityTestRule(mChromeTabbedActivityTestRule)
                        .withIsOpeningTab(true)
                        .withIsSelectingTab(true)
                        .build();

        return Trip.travelSync(
                mStation, destination, () -> NEW_INCOGNITO_TAB_ITEM.perform(click()));
    }
}
