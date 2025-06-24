// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.hub;

import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.containsString;

import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.ViewElement;

/** Facility to represent the declutter message card shown in the regular tab switcher. */
public class ArchiveMessageCardFacility extends Facility<TabSwitcherStation> {
    private final ViewElement mTextElement;

    public ArchiveMessageCardFacility(TabSwitcherStation tabSwitcherStation) {
        mTextElement =
                declareView(
                        tabSwitcherStation.recyclerViewElement.descendant(
                                withText(containsString("inactive item"))));
    }

    public ArchivedTabsDialogStation openArchivedTabsDialog() {
        return mTextElement.clickTo().arriveAt(new ArchivedTabsDialogStation());
    }
}
