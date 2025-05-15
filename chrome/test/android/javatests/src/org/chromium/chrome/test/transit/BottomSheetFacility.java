// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.chromium.base.test.transit.ViewSpec.viewSpec;

import android.view.View;

import androidx.test.espresso.Espresso;

import org.hamcrest.Matcher;

import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.base.test.transit.ViewSpec;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.test.R;

/**
 * Bottom Sheet that appears to move a tab or list of tabs to a tab group.
 *
 * @param <HostStationT> the type of station this is scoped to.
 */
public abstract class BottomSheetFacility<HostStationT extends Station<ChromeTabbedActivity>>
        extends Facility<HostStationT> {
    private static final Matcher<View> BOTTOM_SHEET_MATCHER = withId(R.id.sheet_container);
    protected final ViewElement<View> mBottomSheetContent;

    /** Constructor. Expects a specific title and selected color. */
    public BottomSheetFacility() {
        mBottomSheetContent = declareView(viewSpec(BOTTOM_SHEET_MATCHER));
    }

    protected final <ViewT extends View> ViewElement<ViewT> declareDescendantView(
            ViewSpec<ViewT> viewSpec) {
        ViewSpec<ViewT> descendant =
                mBottomSheetContent.descendant(viewSpec.getViewClass(), viewSpec.getViewMatcher());
        return declareView(descendant);
    }

    /** Press the system backpress to close the bottom sheet. */
    public void close() {
        mHostStation.exitFacilitySync(this, Espresso::pressBack);
    }
}
