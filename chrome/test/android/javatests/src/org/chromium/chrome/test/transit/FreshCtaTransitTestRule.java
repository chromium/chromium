// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import android.content.Intent;

import com.google.errorprone.annotations.CheckReturnValue;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.test.transit.TripBuilder;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.PageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;

/**
 * Rule for integration tests that start a new {@link ChromeTabbedActivity} in each test case.
 *
 * <p>Tests using this can be batched, but the Activity won't be kept between tests; only the
 * process.
 */
@NullMarked
public class FreshCtaTransitTestRule extends BaseCtaTransitTestRule implements TestRule {
    FreshCtaTransitTestRule() {
        super();
    }

    FreshCtaTransitTestRule(ChromeTabbedActivityTestRule testRule) {
        super(testRule);
    }

    @Override
    public Statement apply(Statement statement, Description description) {
        return mActivityTestRule.apply(statement, description);
    }

    /**
     * Start the test in a blank page.
     *
     * @return the active entry {@link PageStation}
     */
    public WebPageStation startOnBlankPage() {
        return ChromeTabbedActivityEntryPoints.startOnBlankPage(mActivityTestRule);
    }

    /**
     * Start the test in a web page served by the test server.
     *
     * @param url the URL of the page to load
     * @return the active entry {@link PageStation}
     */
    public WebPageStation startOnUrl(String url) {
        return ChromeTabbedActivityEntryPoints.startOnUrl(mActivityTestRule, url);
    }

    /**
     * Start the test in a web page served by the test server.
     *
     * @param relativeUrl the relative URL of the page to serve and load
     * @return the active entry {@link PageStation}
     */
    public WebPageStation startOnTestServerUrl(String relativeUrl) {
        assert relativeUrl.startsWith("/") : "|relativeUrl| must be relative";
        String fullUrl = mActivityTestRule.getTestServer().getURL(relativeUrl);
        return ChromeTabbedActivityEntryPoints.startOnUrl(mActivityTestRule, fullUrl);
    }

    /**
     * Start the test from the launcher expecting to show the NTP (given there is no tab state).
     *
     * @return the active entry {@link RegularNewTabPageStation}
     */
    public RegularNewTabPageStation startFromLauncherAtNtp() {
        return ChromeTabbedActivityEntryPoints.startFromLauncherAtNtp(mActivityTestRule);
    }

    /** Start the ChromeTabbedActivity as if it was started from the launcher. */
    @CheckReturnValue
    public static TripBuilder startFromLauncherTo(ChromeTabbedActivityTestRule ctaTestRule) {
        return ChromeTabbedActivityEntryPoints.startFromLauncherTo(ctaTestRule);
    }

    /**
     * Start the test by launching Chrome with a given Intent.
     *
     * @param intent the Intent to launch Chrome with
     */
    @CheckReturnValue
    public TripBuilder startWithIntentTo(Intent intent) {
        return ChromeTabbedActivityEntryPoints.startWithIntentTo(mActivityTestRule, intent);
    }

    /**
     * Start the test by launching Chrome with a given Intent and url.
     *
     * @param intent the Intent to launch Chrome with
     * @param url the URL to add to the Intent
     */
    @CheckReturnValue
    public TripBuilder startWithIntentPlusUrlTo(Intent intent, String url) {
        return ChromeTabbedActivityEntryPoints.startWithIntentPlusUrlTo(
                mActivityTestRule, intent, url);
    }

    /**
     * Start the test in an NTP.
     *
     * @return the active entry {@link RegularNewTabPageStation}
     */
    public RegularNewTabPageStation startOnNtp() {
        return ChromeTabbedActivityEntryPoints.startOnNtp(mActivityTestRule);
    }

    /**
     * Hop onto Public Transit when the test has already started the ChromeTabbedActivity in a blank
     * page.
     *
     * @return the active entry {@link WebPageStation}
     */
    public WebPageStation alreadyStartedOnBlankPage() {
        return ChromeTabbedActivityEntryPoints.alreadyStartedOnBlankPage();
    }
}
