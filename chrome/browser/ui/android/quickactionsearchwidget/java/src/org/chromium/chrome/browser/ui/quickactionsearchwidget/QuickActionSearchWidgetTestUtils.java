// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.quickactionsearchwidget;

import android.app.Activity;
import android.view.View;

import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.runner.lifecycle.Stage;

import org.junit.Assert;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Matchers;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.searchwidget.SearchActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.embedder_support.util.UrlConstants;

/** Util class for testing the Quick Action Search Widget. */
class QuickActionSearchWidgetTestUtils {
    /**
     * Asserts that {@link SearchActivity} is launched in the correct mode after a given {@link
     * Runnable} is ran.
     *
     * @param testRule BaseActivityTestRule used to start an action and monitor for changes.
     * @param action The runnable such that after it is ran, {@link SearchActivity} is expected to
     *     be launched.
     * @param shouldActivityLaunchVoiceMode Whether the search activity is expected to launched in
     *     voice mode or not.
     */
    static void assertSearchActivityLaunchedAfterAction(
            BaseActivityTestRule<Activity> testRule,
            final Runnable action,
            final boolean shouldActivityLaunchVoiceMode) {
        testRule.setActivity(
                ApplicationTestUtils.waitForActivityWithClass(
                        SearchActivity.class, Stage.CREATED, action));
    }

    /**
     * Asserts that {@link ChromeTabbedActivity} is launched with the URL chrome://dino after a
     * given {@link Runnable} is ran.
     *
     * @param testRule BaseActivityTestRule used to start an action and monitor for changes.
     * @param action the runnable such that after running {@link ChromeTabbedActivity} is expected
     *     to be launched.
     */
    static void assertDinoGameLaunchedAfterAction(
            BaseActivityTestRule<Activity> testRule, Runnable action) {
        ChromeTabbedActivity activity =
                ApplicationTestUtils.waitForActivityWithClass(
                        ChromeTabbedActivity.class, Stage.CREATED, action);
        testRule.setActivity(activity);

        CriteriaHelper.pollUiThread(
                () -> {
                    Tab activityTab = activity.getActivityTab();
                    Criteria.checkThat(activityTab, Matchers.notNullValue());
                    Criteria.checkThat(activityTab.getUrl(), Matchers.notNullValue());
                    Criteria.checkThat(
                            activityTab.getUrl().getSpec(),
                            Matchers.equalTo(UrlConstants.CHROME_DINO_URL));
                });
    }

    /**
     * Click on a view by id.
     *
     * @param view the view containing the click target.
     * @param clickTarget the id of the view to click on.
     */
    static void clickOnView(final View view, final int clickTarget) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    view.findViewById(clickTarget).performClick();
                });
    }

    /**
     * Asserts that {@link ChromeTabbedActivity} is launched in an incognito mode after a given
     * {@link Runnable} is ran.
     *
     * @param testRule BaseActivityTestRule used to start an action and monitor for changes.
     * @param action the runnable such that after running {@link ChromeTabbedActivity} is expected
     *     to be launched.
     */
    public static void assertIncognitoModeLaunchedAfterAction(
            BaseActivityTestRule<Activity> testRule, Runnable action) {
        ChromeTabbedActivity activity =
                ApplicationTestUtils.waitForActivityWithClass(
                        ChromeTabbedActivity.class, Stage.CREATED, action);
        testRule.setActivity(activity);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        CriteriaHelper.pollUiThread(
                () -> {
                    Tab activityTab = activity.getActivityTab();
                    Assert.assertTrue(activity.getTabModelSelector().isIncognitoSelected());
                    Criteria.checkThat(activityTab, Matchers.notNullValue());
                    Criteria.checkThat(
                            activityTab.getUrl().getSpec(),
                            Matchers.startsWith(UrlConstants.NTP_URL));
                });
    }
}
