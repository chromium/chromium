// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.CoreMatchers.allOf;

import static org.chromium.base.test.transit.ViewElement.scopedViewElement;

import androidx.test.espresso.Espresso;

import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;

/** The 3-dot menu "Select Tabs" UI for the {@link HubTabSwitcherBase} panes. */
// TODO(crbug/324919909): Migrate TabListEditorTestingRobot to here.
public class HubTabSwitcherListEditorFacility extends Facility<HubTabSwitcherBaseStation> {
    public static final ViewElement TAB_LIST_EDITOR_LAYOUT =
            scopedViewElement(withId(R.id.selectable_list));
    public static final ViewElement TAB_LIST_EDITOR_RECYCLER_VIEW =
            scopedViewElement(
                    allOf(
                            isDescendantOfA(withId(R.id.selectable_list)),
                            withId(R.id.tab_list_recycler_view)));

    private final ChromeTabbedActivityTestRule mChromeTabbedActivityTestRule;

    public HubTabSwitcherListEditorFacility(
            HubTabSwitcherBaseStation station,
            ChromeTabbedActivityTestRule chromeTabbedActivityTestRule) {
        super(station);
        mChromeTabbedActivityTestRule = chromeTabbedActivityTestRule;
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        elements.declareView(TAB_LIST_EDITOR_LAYOUT);
        elements.declareView(TAB_LIST_EDITOR_RECYCLER_VIEW);
    }

    /** Presses back to exit the facility. */
    public void pressBackToExit() {
        Facility.exitSync(
                this,
                () -> {
                    Espresso.pressBack();
                });
    }
}
