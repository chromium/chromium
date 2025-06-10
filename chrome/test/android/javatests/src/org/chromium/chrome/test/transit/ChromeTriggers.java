// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import androidx.test.platform.app.InstrumentationRegistry;

import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.TripBuilder;
import org.chromium.chrome.browser.app.ChromeActivity;
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
    public static TripBuilder invokeCustomMenuActionTo(
            int menuId, Station<? extends ChromeActivity> station) {
        return station.runTo(
                () ->
                        MenuUtils.invokeCustomMenuActionSync(
                                InstrumentationRegistry.getInstrumentation(),
                                station.getActivity(),
                                menuId));
    }
}
