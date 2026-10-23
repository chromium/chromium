// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import androidx.test.platform.app.InstrumentationRegistry;

import com.google.errorprone.annotations.CheckReturnValue;

import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.TripBuilder;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.tabmodel.TabClosingSource;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.transit.hub.RegularTabSwitcherStation;
import org.chromium.chrome.test.transit.hub.UndoSnackbarFacility;
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

    /**
     * Closes all regular and incognito tabs programmatically and arrives at the regular tab
     * switcher.
     *
     * @param station The current Station.
     * @param facilities Facilities expected after closing, e.g. an {@link UndoSnackbarFacility}.
     */
    public static RegularTabSwitcherStation closeAllTabsProgrammatically(
            Station<ChromeTabbedActivity> station, Facility<?>... facilities) {
        TabModelSelector selector = station.getActivity().getTabModelSelector();
        return station.runOnUiThreadTo(
                        () -> {
                            TabClosureParams params =
                                    TabClosureParams.closeAllTabs()
                                            .uponExit(false)
                                            .tabClosingSource(TabClosingSource.GRID_TAB_SWITCHER)
                                            .build();
                            selector.getModel(/* incognito= */ false)
                                    .getTabRemover()
                                    .closeTabs(params, /* allowDialog= */ false);
                            selector.getModel(/* incognito= */ true)
                                    .getTabRemover()
                                    .closeTabs(params, /* allowDialog= */ false);
                        })
                .arriveAt(
                        new RegularTabSwitcherStation(
                                /* regularTabsExist= */ false, /* incognitoTabsExist= */ false),
                        facilities);
    }

    /**
     * Close the current tab of |activity| programmatically, without undo.
     *
     * <p>Takes the activity separately from |station| for Stations that are not bound to a
     * ChromeTabbedActivity, e.g. a Settings page shown in a tab.
     */
    @CheckReturnValue
    public static TripBuilder closeCurrentTabProgrammaticallyTo(
            Station<?> station, ChromeTabbedActivity activity) {
        return station.runOnUiThreadTo(
                () ->
                        activity.getCurrentTabModel()
                                .getTabRemover()
                                .closeTabs(
                                        TabClosureParams.closeTab(activity.getActivityTab())
                                                .allowUndo(false)
                                                .build(),
                                        /* allowDialog= */ false));
    }
}
