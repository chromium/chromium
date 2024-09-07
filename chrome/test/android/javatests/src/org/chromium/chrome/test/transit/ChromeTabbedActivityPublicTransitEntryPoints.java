// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import org.chromium.base.test.transit.BatchedPublicTransitRule;
import org.chromium.base.test.transit.EntryPointSentinelStation;
import org.chromium.base.test.transit.Station;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.PageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.components.embedder_support.util.UrlConstants;

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
        EntryPointSentinelStation sentinel = new EntryPointSentinelStation();
        sentinel.setAsEntryPoint();

        WebPageStation entryPageStation = WebPageStation.newBuilder().withEntryPoint().build();
        return sentinel.travelToSync(
                entryPageStation, mActivityTestRule::startMainActivityOnBlankPage);
    }

    /**
     * Start the test in an NTP.
     *
     * @return the active entry {@link RegularNewTabPageStation}
     */
    public RegularNewTabPageStation startOnNtpNonBatched() {
        EntryPointSentinelStation sentinel = new EntryPointSentinelStation();
        sentinel.setAsEntryPoint();
        RegularNewTabPageStation entryPageStation =
                RegularNewTabPageStation.newBuilder().withEntryPoint().build();
        return sentinel.travelToSync(
                entryPageStation,
                () -> mActivityTestRule.startMainActivityWithURL(UrlConstants.NTP_URL));
    }

    /**
     * Start the batched test in a blank page.
     *
     * @return the active entry {@link WebPageStation}
     */
    public WebPageStation startOnBlankPage(BatchedPublicTransitRule<WebPageStation> batchedRule) {
        return startBatched(batchedRule, this::startOnBlankPageNonBatched);
    }

    /**
     * Start the batched test in the New Tab Page.
     *
     * @return the active entry {@link RegularNewTabPageStation}
     */
    public RegularNewTabPageStation startOnNtp(
            BatchedPublicTransitRule<RegularNewTabPageStation> batchedRule) {
        return startBatched(batchedRule, this::startOnNtpNonBatched);
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

    /**
     * Hop onto Public Transit when the test has already started the ChromeTabbedActivity in a blank
     * page.
     *
     * @return the active entry {@link WebPageStation}
     */
    public WebPageStation alreadyStartedOnBlankPageNonBatched() {
        EntryPointSentinelStation sentinel = new EntryPointSentinelStation();
        sentinel.setAsEntryPoint();

        WebPageStation entryPageStation = WebPageStation.newBuilder().withEntryPoint().build();
        return sentinel.travelToSync(entryPageStation, /* trigger= */ null);
    }
}
