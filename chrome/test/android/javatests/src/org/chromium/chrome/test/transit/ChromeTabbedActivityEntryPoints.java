// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import android.content.Intent;

import org.chromium.base.test.transit.EntryPointSentinelStation;
import org.chromium.base.test.transit.Station;
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

    /** Start the ChromeTabbedActivity in a web page at the given |url|. */
    public static WebPageStation startOnUrl(ChromeTabbedActivityTestRule ctaTestRule, String url) {
        EntryPointSentinelStation sentinel = new EntryPointSentinelStation();
        sentinel.setAsEntryPoint();

        WebPageStation entryPageStation =
                WebPageStation.newBuilder().withEntryPoint().withExpectedUrlSubstring(url).build();
        return sentinel.travelToSync(
                entryPageStation, () -> ctaTestRule.startMainActivityWithURL(url));
    }

    /** Start the ChromeTabbedActivity in an NTP as if it was started from the launcher. */
    public static RegularNewTabPageStation startFromLauncher(
            ChromeTabbedActivityTestRule ctaTestRule) {
        EntryPointSentinelStation sentinel = new EntryPointSentinelStation();
        sentinel.setAsEntryPoint();
        RegularNewTabPageStation entryPageStation =
                RegularNewTabPageStation.newBuilder().withEntryPoint().build();
        return sentinel.travelToSync(entryPageStation, ctaTestRule::startMainActivityFromLauncher);
    }

    /**
     * Start the ChromeTabbedActivity in an NTP as if receiving an Intent to view
     * "chrome-native://newtab/".
     */
    public static RegularNewTabPageStation startOnNtp(ChromeTabbedActivityTestRule ctaTestRule) {
        EntryPointSentinelStation sentinel = new EntryPointSentinelStation();
        sentinel.setAsEntryPoint();
        RegularNewTabPageStation entryPageStation =
                RegularNewTabPageStation.newBuilder().withEntryPoint().build();
        return sentinel.travelToSync(
                entryPageStation, () -> ctaTestRule.startMainActivityWithURL(UrlConstants.NTP_URL));
    }

    /**
     * Start the ChromeTabbedActivity with an Intent.
     *
     * <p>The caller needs to specify the expected state reached by passing |expectedStation|.
     */
    public static <T extends Station<?>> T startWithIntent(
            ChromeTabbedActivityTestRule ctaTestRule, Intent intent, T expectedStation) {
        EntryPointSentinelStation sentinel = new EntryPointSentinelStation();
        sentinel.setAsEntryPoint();
        return sentinel.travelToSync(
                expectedStation, () -> ctaTestRule.startActivityCompletely(intent));
    }

    /**
     * Start the ChromeTabbedActivity with an Intent, adding a URL to it.
     *
     * <p>The caller needs to specify the expected state reached by passing |expectedStation|.
     */
    public static <T extends Station<?>> T startWithIntentPlusUrl(
            ChromeTabbedActivityTestRule ctaTestRule,
            Intent intent,
            String url,
            T expectedStation) {
        EntryPointSentinelStation sentinel = new EntryPointSentinelStation();
        sentinel.setAsEntryPoint();
        return sentinel.travelToSync(
                expectedStation, () -> ctaTestRule.startMainActivityFromIntent(intent, url));
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
