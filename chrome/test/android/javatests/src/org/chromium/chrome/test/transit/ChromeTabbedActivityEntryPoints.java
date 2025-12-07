// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import android.content.Intent;

import com.google.errorprone.annotations.CheckReturnValue;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.transit.EntryPointSentinelStation;
import org.chromium.base.test.transit.TripBuilder;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
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
        disableFirstRunFlow();

        EntryPointSentinelStation sentinel = new EntryPointSentinelStation();
        sentinel.setAsEntryPoint();

        return sentinel.runTo(ctaTestRule::startMainActivityOnBlankPage)
                .arriveAt(WebPageStation.newBuilder().withEntryPoint().build());
    }

    /** Start the ChromeTabbedActivity in an incognito blank page. */
    public static WebPageStation startOnIncognitoBlankPage(
            ChromeTabbedActivityTestRule ctaTestRule) {
        disableFirstRunFlow();

        EntryPointSentinelStation sentinel = new EntryPointSentinelStation();
        sentinel.setAsEntryPoint();

        return sentinel.runTo(ctaTestRule::startMainActivityOnIncognitoBlankPage)
                .arriveAt(WebPageStation.newBuilder().withEntryPoint().withIncognito(true).build());
    }

    /** Start the ChromeTabbedActivity in a web page at the given |url|. */
    public static WebPageStation startOnUrl(ChromeTabbedActivityTestRule ctaTestRule, String url) {
        return startOnUrlTo(ctaTestRule, url)
                .arriveAt(
                        WebPageStation.newBuilder()
                                .withEntryPoint()
                                .withExpectedUrlSubstring(url)
                                .build());
    }

    /** Start the ChromeTabbedActivity with the given |url|. */
    @CheckReturnValue
    public static TripBuilder startOnUrlTo(ChromeTabbedActivityTestRule ctaTestRule, String url) {
        disableFirstRunFlow();

        EntryPointSentinelStation sentinel = new EntryPointSentinelStation();
        sentinel.setAsEntryPoint();

        return sentinel.runTo(() -> ctaTestRule.startMainActivityWithURL(url));
    }

    /** Start the ChromeTabbedActivity in an NTP as if it was started from the launcher. */
    public static RegularNewTabPageStation startFromLauncherAtNtp(
            ChromeTabbedActivityTestRule ctaTestRule) {
        return startFromLauncherTo(ctaTestRule)
                .arriveAt(RegularNewTabPageStation.newBuilder().withEntryPoint().build());
    }

    /** Start the ChromeTabbedActivity as if it was started from the launcher. */
    @CheckReturnValue
    public static TripBuilder startFromLauncherTo(ChromeTabbedActivityTestRule ctaTestRule) {
        disableFirstRunFlow();

        EntryPointSentinelStation sentinel = new EntryPointSentinelStation();
        sentinel.setAsEntryPoint();

        return sentinel.runTo(ctaTestRule::startMainActivityFromLauncher);
    }

    /**
     * Start the ChromeTabbedActivity in an NTP as if receiving an Intent to view
     * "chrome-native://newtab/".
     */
    public static RegularNewTabPageStation startOnNtp(ChromeTabbedActivityTestRule ctaTestRule) {
        disableFirstRunFlow();

        EntryPointSentinelStation sentinel = new EntryPointSentinelStation();
        sentinel.setAsEntryPoint();

        return sentinel.runTo(() -> ctaTestRule.startMainActivityWithURL(UrlConstants.NTP_URL))
                .arriveAt(RegularNewTabPageStation.newBuilder().withEntryPoint().build());
    }

    /** Start the ChromeTabbedActivity with an Intent. */
    @CheckReturnValue
    public static TripBuilder startWithIntentTo(
            ChromeTabbedActivityTestRule ctaTestRule, Intent intent) {
        disableFirstRunFlow();

        EntryPointSentinelStation sentinel = new EntryPointSentinelStation();
        sentinel.setAsEntryPoint();
        return sentinel.runTo(() -> ctaTestRule.startActivityCompletely(intent));
    }

    /** Start the ChromeTabbedActivity with an Intent, adding a URL to it. */
    @CheckReturnValue
    public static TripBuilder startWithIntentPlusUrlTo(
            ChromeTabbedActivityTestRule ctaTestRule, Intent intent, @Nullable String url) {
        disableFirstRunFlow();

        EntryPointSentinelStation sentinel = new EntryPointSentinelStation();
        sentinel.setAsEntryPoint();

        return sentinel.runTo(() -> ctaTestRule.startMainActivityFromIntent(intent, url));
    }

    /**
     * Hop onto Public Transit when the test has already started the ChromeTabbedActivity in a blank
     * page.
     */
    public static WebPageStation alreadyStartedOnBlankPage() {
        EntryPointSentinelStation sentinel = new EntryPointSentinelStation();
        sentinel.setAsEntryPoint();

        return sentinel.noopTo().arriveAt(WebPageStation.newBuilder().withEntryPoint().build());
    }

    private static void disableFirstRunFlow() {
        ThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(true));
    }
}
