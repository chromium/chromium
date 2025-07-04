// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.ui;

import static androidx.test.espresso.matcher.ViewMatchers.withId;

import android.view.View;

import org.hamcrest.Matcher;

import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.test.R;

/**
 * Bottom Sheet that appears to move a tab or list of tabs to a tab group.
 *
 * @param <HostStationT> the type of station this is scoped to.
 */
public abstract class BottomSheetFacility<HostStationT extends Station<ChromeTabbedActivity>>
        extends Facility<HostStationT> {
    public final ViewElement<View> bottomSheetElement;

    /** Constructor. Expects a specific title and selected color. */
    public BottomSheetFacility() {
        bottomSheetElement = declareView(withId(R.id.sheet_container));
    }

    @SafeVarargs
    protected final ViewElement<View> declareDescendantView(Matcher<View>... viewMatchers) {
        return declareView(bottomSheetElement.descendant(viewMatchers));
    }

    @SafeVarargs
    protected final <ViewT extends View> ViewElement<ViewT> declareDescendantView(
            Class<ViewT> viewClass, Matcher<View>... viewMatchers) {
        return declareView(bottomSheetElement.descendant(viewClass, viewMatchers));
    }

    /** Press the system backpress to close the bottom sheet. */
    public void close() {
        pressBackTo().exitFacility();
    }
}
