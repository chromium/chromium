// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import androidx.test.platform.app.InstrumentationRegistry;

import com.google.errorprone.annotations.CheckReturnValue;

import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.TripBuilder;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.test.util.MenuUtils;

/** Collection of Chrome-specific Triggers to start Transitions. */
public class ChromeTriggers {
    /**
     * Invoke a menu action from the app menu to perform a Transition.
     *
     * <p>Use this when the App menu is just a means to test something else. When the App Menu is
     * relevant, use one of the {@link AppMenuFacility} subclasses instead. This trigger calls the
     * handler directly and doesn't actually open the app menu. The direct call is faster since no
     * UI is involved and a good shortcut to speed up tests.
     */
    @CheckReturnValue
    public static TripBuilder invokeCustomMenuActionTo(
            int menuId, Station<? extends ChromeActivity> station) {
        return station.runTo(
                () ->
                        MenuUtils.invokeCustomMenuActionSync(
                                InstrumentationRegistry.getInstrumentation(),
                                station.getActivity(),
                                menuId));
    }

    /** Switch to the browsing layout programmatically. */
    @CheckReturnValue
    public static TripBuilder showBrowsingLayoutTo(
            Station<? extends ChromeTabbedActivity> station) {
        return station.runOnUiThreadTo(
                () ->
                        station.getActivity()
                                .getLayoutManager()
                                .showLayout(LayoutType.BROWSING, /* animate= */ false));
    }

    /** Switch to the tab switcher layout programmatically. */
    @CheckReturnValue
    public static TripBuilder showTabSwitcherLayoutTo(
            Station<? extends ChromeTabbedActivity> station) {
        return station.runOnUiThreadTo(
                () ->
                        station.getActivity()
                                .getLayoutManager()
                                .showLayout(LayoutType.TAB_SWITCHER, /* animate= */ false));
    }

    /** Close all tabs in the current tab model programmatically. */
    @CheckReturnValue
    public static TripBuilder closeAllTabsProgrammaticallyTo(
            Station<? extends ChromeTabbedActivity> station) {
        return station.runOnUiThreadTo(
                () ->
                        station.getActivity()
                                .getCurrentTabModel()
                                .getTabRemover()
                                .closeTabs(
                                        TabClosureParams.closeAllTabs().build(),
                                        /* allowDialog= */ false));
    }
}
