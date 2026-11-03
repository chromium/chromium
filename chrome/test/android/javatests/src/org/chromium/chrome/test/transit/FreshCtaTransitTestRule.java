// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import android.content.Intent;

import com.google.errorprone.annotations.CheckReturnValue;

import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.transit.TrafficControl;
import org.chromium.base.test.transit.TripBuilder;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.BasePageStation;
import org.chromium.chrome.test.transit.page.CtaPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;

/**
 * Rule for integration tests that start a new {@link ChromeTabbedActivity} in each test case.
 *
 * <p>This rule closes all windows at the end of the test.
 *
 * <p>Tests using this can be batched, but the Activity won't be kept between tests; only the
 * process.
 */
@NullMarked
public class FreshCtaTransitTestRule extends BaseCtaTransitTestRule {
    private static final String TAG = "TestRule";
    private boolean mSkipInstanceAndTabStateCleanup;

    /** Use {@link ChromeTransitTestRules#freshChromeTabbedActivityRule()}. */
    FreshCtaTransitTestRule() {
        super();
    }

    /** Use {@link ChromeTransitTestRules#wrapTestRule(ChromeTabbedActivityTestRule)}. */
    FreshCtaTransitTestRule(ChromeTabbedActivityTestRule testRule) {
        super(testRule);
    }

    @Override
    public Statement apply(Statement statement, Description description) {
        Statement thisStatement =
                new Statement() {
                    @Override
                    public void evaluate() throws Throwable {
                        mSkipInstanceAndTabStateCleanup = false;
                        Throwable failure = null;
                        try {
                            statement.evaluate();
                        } catch (Throwable t) {
                            failure = t;
                        }
                        try {
                            // crbug.com/460433346: Skip this clean up to avoid issues in some
                            // tests, mostly post tasks running after instance and tab state
                            // clean up on destroyed Activities.
                            if (!mSkipInstanceAndTabStateCleanup) {
                                closeAllWindowsAndDeleteInstanceAndTabState();
                            }
                        } catch (Throwable t) {
                            // Keep the test's own failure as the primary one.
                            if (failure == null) {
                                failure = t;
                            } else {
                                failure.addSuppressed(t);
                            }
                        } finally {
                            TrafficControl.hopOffPublicTransit();
                        }
                        if (failure != null) throw failure;
                    }
                };

        return mActivityTestRule.apply(thisStatement, description);
    }

    /** Close all Activities, cleaning up instance and tab state of ChromeTabbedActivities. */
    public void closeAllWindowsAndDeleteInstanceAndTabState() {
        finishActivitiesWithCleanup(ApplicationStatus.getRunningActivities());
    }

    @Override
    public WebPageStation startOnBlankPage() {
        return ChromeTabbedActivityEntryPoints.startOnBlankPage(mActivityTestRule);
    }

    /**
     * Start the test in an incognito blank page.
     *
     * @return the active entry {@link CtaPageStation}
     */
    public WebPageStation startOnIncognitoBlankPage() {
        return ChromeTabbedActivityEntryPoints.startOnIncognitoBlankPage(mActivityTestRule);
    }

    @Override
    public <T extends CtaPageStation> T startOnPage(
            String url, BasePageStation.Builder<T> builder) {
        return startOnUrlTo(url).arriveAt(builder.initForEntryPointUrl(url).build());
    }

    /**
     * Start the test with a url that leads to a web page.
     *
     * @param url the URL of the page to load
     * @return the active entry {@link CtaPageStation}
     */
    public WebPageStation startOnUrl(String url) {
        return startOnWebPage(url);
    }

    /**
     * Start the test with a url.
     *
     * @param url the URL of the page to load
     * @return a {@link TripBuilder} to complete the transition.
     */
    @CheckReturnValue
    public TripBuilder startOnUrlTo(String url) {
        return ChromeTabbedActivityEntryPoints.startOnUrlTo(mActivityTestRule, url);
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
    public TripBuilder startFromLauncherTo() {
        return ChromeTabbedActivityEntryPoints.startFromLauncherTo(mActivityTestRule);
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
    public TripBuilder startWithIntentPlusUrlTo(Intent intent, @Nullable String url) {
        return ChromeTabbedActivityEntryPoints.startWithIntentPlusUrlTo(
                mActivityTestRule, intent, url);
    }

    /**
     * Start the test by launching Chrome with a given Intent and url expecting to show a webpage.
     *
     * @param intent the Intent to launch Chrome with
     * @param url the URL to add to the Intent
     */
    public WebPageStation startWithIntentPlusUrlAtWebPage(Intent intent, String url) {
        return ChromeTabbedActivityEntryPoints.startWithIntentPlusUrlTo(
                        mActivityTestRule, intent, url)
                .arriveAt(
                        WebPageStation.newBuilder()
                                .withEntryPoint()
                                .withExpectedUrlSubstring(url)
                                .build());
    }

    @Override
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

    /**
     * Avoid closing the {@link ChromeTabbedActivity} and cleaning up tab state and instances after
     * the test.
     */
    public void skipWindowAndTabStateCleanup() {
        mSkipInstanceAndTabStateCleanup = true;
    }
}
