// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import org.chromium.base.test.transit.Trip;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;

/** Entry points for Public Transit tests that use ChromeTabbedActivity. */
public class ChromeTabbedActivityPublicTransitEntryPoints {
    private final ChromeTabbedActivityTestRule mChromeTabbedActivityTestRule;

    public ChromeTabbedActivityPublicTransitEntryPoints(
            ChromeTabbedActivityTestRule chromeTabbedActivityTestRule) {
        mChromeTabbedActivityTestRule = chromeTabbedActivityTestRule;
    }

    /**
     * Start the test in a blank page.
     *
     * @return the active {@link EntryPageStation}
     */
    public EntryPageStation startOnBlankPage() {
        EntryPageStation entryPageStation =
                new EntryPageStation(mChromeTabbedActivityTestRule, false);
        return Trip.travelSync(
                null,
                entryPageStation,
                (t) -> mChromeTabbedActivityTestRule.startMainActivityOnBlankPage());
    }
}
