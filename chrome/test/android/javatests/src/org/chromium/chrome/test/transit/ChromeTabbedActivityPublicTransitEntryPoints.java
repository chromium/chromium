// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import org.chromium.base.test.transit.BatchedPublicTransitRule;
import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.Trip;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;

import java.util.concurrent.Callable;

/** Entry points for Public Transit tests that use ChromeTabbedActivity. */
public class ChromeTabbedActivityPublicTransitEntryPoints {
    private final ChromeTabbedActivityTestRule mActivityTestRule;
    private static ChromeTabbedActivity sActivity;

    public ChromeTabbedActivityPublicTransitEntryPoints(
            ChromeTabbedActivityTestRule chromeTabbedActivityTestRule) {
        mActivityTestRule = chromeTabbedActivityTestRule;
    }

    /**
     * Start the test in a blank page.
     *
     * @return the active entry {@link PageStation}
     */
    public WebPageStation startOnBlankPageNonBatched() {
        WebPageStation entryPageStation =
                WebPageStation.newWebPageStationBuilder()
                        .withActivityTestRule(mActivityTestRule)
                        .withEntryPoint()
                        .build();
        return Trip.travelSync(
                null, entryPageStation, () -> mActivityTestRule.startMainActivityOnBlankPage());
    }

    /**
     * Start the batched test in a blank page.
     *
     * @return the active entry {@link PageStation}
     */
    public PageStation startOnBlankPage(BatchedPublicTransitRule<PageStation> batchedRule) {
        return startBatched(batchedRule, this::startOnBlankPageNonBatched);
    }

    private <T extends Station> T startBatched(
            BatchedPublicTransitRule<T> batchedRule, Callable<T> entryPointCallable) {
        mActivityTestRule.setFinishActivity(false);
        T station = batchedRule.getHomeStation();
        if (station == null) {
            try {
                station = entryPointCallable.call();
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
            sActivity = mActivityTestRule.getActivity();
        } else {
            mActivityTestRule.setActivity(sActivity);
        }
        return station;
    }
}
