// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.quickactionsearchwidget;

import android.app.Activity;
import android.content.Intent;
import android.support.test.InstrumentationRegistry;
import android.view.View;

import org.junit.Assert;

import org.chromium.base.IntentUtils;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Matchers;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.searchwidget.SearchActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityConstants;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Util class for testing the Quick Action Search Widget.
 */
class QuickActionSearchWidgetTestUtils {
    /**
     * Asserts that {@link SearchActivity} is launched in the correct mode after a given {@link
     * Runnable} is ran.
     *
     * @param action The runnable such that after it is ran, {@link SearchActivity} is expected to
     *         be launched.
     * @param shouldActivityLaunchVoiceMode Whether the search activity is expected to launched in
     *         voice mode or not.
     */
    static void assertSearchActivityLaunchedAfterAction(
            final Runnable action, final boolean shouldActivityLaunchVoiceMode) {
        Activity activity = ActivityTestUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), SearchActivity.class, action);

        Assert.assertNotNull(activity);
        assertSearchActivityLaunchedWithCorrectVoiceExtras(activity, shouldActivityLaunchVoiceMode);
    }

    /**
     * Asserts that {@link ChromeTabbedActivity} is launched with the URL chrome://dino after a
     * given {@link Runnable} is ran.
     *
     * @param action the runnable such that after running {@link ChromeTabbedActivity} is expected
     *         to be launched.
     */
    static void assertDinoGameLaunchedAfterAction(Runnable action) {
        final ChromeTabbedActivity activity = ActivityTestUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), ChromeTabbedActivity.class, action);

        Assert.assertNotNull(activity);

        CriteriaHelper.pollUiThread(() -> {
            Tab activityTab = activity.getActivityTab();
            Criteria.checkThat(activityTab, Matchers.notNullValue());
            Criteria.checkThat(activityTab.getUrl(), Matchers.notNullValue());
            Criteria.checkThat(activityTab.getUrl().getSpec(),
                    Matchers.startsWith(UrlConstants.CHROME_DINO_URL));
        });
    }

    /**
     * Click on a view by id.
     *
     * @param view the view containing the click target.
     * @param clickTarget the id of the view to click on.
     */
    static void clickOnView(final View view, final int clickTarget) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { view.findViewById(clickTarget).performClick(); });
    }

    private static void assertSearchActivityLaunchedWithCorrectVoiceExtras(
            final Activity activity, final boolean shouldActivityLaunchVoiceMode) {
        Intent intent = activity.getIntent();
        boolean isVoiceMode = IntentUtils.safeGetBooleanExtra(
                intent, SearchActivityConstants.EXTRA_SHOULD_START_VOICE_SEARCH, false);
        Assert.assertEquals(shouldActivityLaunchVoiceMode, isVoiceMode);
    }
}
