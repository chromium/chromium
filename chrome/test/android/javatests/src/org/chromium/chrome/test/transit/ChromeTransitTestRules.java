// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import androidx.core.util.Function;

import org.chromium.base.test.transit.Station;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;

/** Factory methods for Public Transit test rules. */
public class ChromeTransitTestRules {
    /** Starts each test case in a fresh ChromeTabbedActivity. */
    public static FreshCtaTransitTestRule freshChromeTabbedActivityRule() {
        return new FreshCtaTransitTestRule();
    }

    /** Starts each test case in a blank page reusing a ChromeTabbedActivity within the batch. */
    public static ReusedCtaTransitTestRule<WebPageStation> blankPageStartReusedActivityRule() {
        return new ReusedCtaTransitTestRule<>(
                WebPageStation.class, ChromeTabbedActivityEntryPoints::startOnBlankPage);
    }

    /** Starts each test case in an NTP in a reused ChromeTabbedActivity within the batch. */
    public static ReusedCtaTransitTestRule<RegularNewTabPageStation> ntpStartReusedActivityRule() {
        return new ReusedCtaTransitTestRule<>(
                RegularNewTabPageStation.class, ChromeTabbedActivityEntryPoints::startOnNtp);
    }

    /** Auto reset the state of the ChromeTabbedActivity and reuse it within the batch. */
    public static AutoResetCtaTransitTestRule autoResetCtaActivityRule() {
        return new AutoResetCtaTransitTestRule(/* clearAllTabState= */ true);
    }

    /**
     * Auto reset the state of the ChromeTabbedActivity (keeping one tab open) and reuse it within
     * the batch.
     *
     * <p>This is faster than autoResetCtaActivityRule() but might keep more state related to the
     * tab that's kept open.
     */
    public static AutoResetCtaTransitTestRule fastAutoResetCtaActivityRule() {
        return new AutoResetCtaTransitTestRule(/* clearAllTabState= */ false);
    }

    /**
     * Starts each test case in a fresh ChromeTabbedActivity.
     *
     * <p>Use when the test requires a specific subclass of ChromeTabbedActivityTestRule.
     */
    public static FreshCtaTransitTestRule wrapTestRule(ChromeTabbedActivityTestRule rule) {
        return new FreshCtaTransitTestRule(rule);
    }

    /**
     * Starts each test case reusing ChromeTabbedActivity within the batch, in a Station not
     * supported by other methods.
     */
    public static <ST extends Station<ChromeTabbedActivity>>
            ReusedCtaTransitTestRule<ST> customStartReusedActivityRule(
                    Class<ST> hostStationType,
                    Function<ChromeTabbedActivityTestRule, ST> entryPointFunction) {
        return new ReusedCtaTransitTestRule<>(hostStationType, entryPointFunction);
    }
}
