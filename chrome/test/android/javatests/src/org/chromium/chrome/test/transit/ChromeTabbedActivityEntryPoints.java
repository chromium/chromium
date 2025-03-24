// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import org.chromium.base.test.transit.EntryPointSentinelStation;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.components.embedder_support.util.UrlConstants;

/**
 * Public Transit entry points for {@link ChromeTabbedActivity}.
 *
 * <p>Use {@link ChromeTransitTestRules} to create test rules.
 */
public class ChromeTabbedActivityEntryPoints {
    /** Start the ChromeTabbedActivity in a blank page. */
    public static WebPageStation startOnBlankPage(ChromeTabbedActivityTestRule ctaTestRule) {
        EntryPointSentinelStation sentinel = new EntryPointSentinelStation();
        sentinel.setAsEntryPoint();

        WebPageStation entryPageStation = WebPageStation.newBuilder().withEntryPoint().build();
        return sentinel.travelToSync(entryPageStation, ctaTestRule::startMainActivityOnBlankPage);
    }

    /** Start the ChromeTabbedActivity in an NTP. */
    public static RegularNewTabPageStation startOnNtp(ChromeTabbedActivityTestRule ctaTestRule) {
        EntryPointSentinelStation sentinel = new EntryPointSentinelStation();
        sentinel.setAsEntryPoint();
        RegularNewTabPageStation entryPageStation =
                RegularNewTabPageStation.newBuilder().withEntryPoint().build();
        return sentinel.travelToSync(
                entryPageStation, () -> ctaTestRule.startMainActivityWithURL(UrlConstants.NTP_URL));
    }

    /**
     * Hop onto Public Transit when the test has already started the ChromeTabbedActivity in a blank
     * page.
     */
    public static WebPageStation alreadyStartedOnBlankPage() {
        EntryPointSentinelStation sentinel = new EntryPointSentinelStation();
        sentinel.setAsEntryPoint();

        WebPageStation entryPageStation = WebPageStation.newBuilder().withEntryPoint().build();
        return sentinel.travelToSync(entryPageStation, /* trigger= */ null);
    }
}
