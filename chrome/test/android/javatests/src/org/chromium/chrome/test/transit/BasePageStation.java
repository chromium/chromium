// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.longClick;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import android.view.View;

import org.hamcrest.Matcher;

import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.StationFacility;
import org.chromium.base.test.transit.TransitStation;
import org.chromium.base.test.transit.Trip;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;

/**
 * Base class for the screen that shows a webpage with the omnibox and the toolbar.
 *
 * <p>Use the derived {@link PageStation} or {@link EntryPageStation}.
 */
public abstract class BasePageStation extends TransitStation {
    public static final Matcher<View> TAB_SWITCHER_BUTTON = withId(R.id.tab_switcher_button);

    protected final ChromeTabbedActivityTestRule mChromeTabbedActivityTestRule;
    protected final boolean mIncognito;

    protected BasePageStation(
            ChromeTabbedActivityTestRule chromeTabbedActivityTestRule, boolean incognito) {
        mChromeTabbedActivityTestRule = chromeTabbedActivityTestRule;
        mIncognito = incognito;
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        elements.declareUnownedView(TAB_SWITCHER_BUTTON);
    }

    /** Long presses the tab switcher button to open the action menu. */
    public TabSwitcherActionMenuFacility openTabSwitcherActionMenu() {
        recheckEnterConditions();

        TabSwitcherActionMenuFacility menu =
                new TabSwitcherActionMenuFacility(this, mChromeTabbedActivityTestRule);
        return StationFacility.enterSync(
                menu, (e) -> onView(TAB_SWITCHER_BUTTON).perform(longClick()));
    }

    public AppMenuFacility openAppMenu() {
        recheckEnterConditions();

        AppMenuFacility menu = new AppMenuFacility(this, mChromeTabbedActivityTestRule);

        // TODO(crbug.com/1489724): Put a real trigger, the app menu doesn't currently show on the
        // screen.
        return StationFacility.enterSync(menu, (e) -> {});
    }

    /** Opens the tab switcher by pressing the toolbar tab switcher button. */
    public TabSwitcherStation openTabSwitcher() {
        recheckEnterConditions();

        TabSwitcherStation destination = new TabSwitcherStation(mChromeTabbedActivityTestRule);
        return Trip.goSync(this, destination, (e) -> onView(TAB_SWITCHER_BUTTON).perform(click()));
    }

    protected ChromeTabbedActivity getChromeTabbedActivity() {
        ChromeTabbedActivity activity = mChromeTabbedActivityTestRule.getActivity();
        if (activity == null) {
            throw new IllegalStateException("Activity has not yet been created.");
        }
        return activity;
    }
}
