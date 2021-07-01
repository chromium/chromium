// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.quickactionsearchwidget;

import android.app.Activity;
import android.support.test.InstrumentationRegistry;
import android.view.View;

import org.junit.Assert;

import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Matchers;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.searchwidget.SearchActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Util class for testing the Quick Action Search Widget.
 */
class QuickActionSearchWidgetTestUtils {
    /**
     * Asserts that {@link SearchActivity} is launched after a given {@link Runnable} is ran.
     *
     * @param action the runnable such that after running {@link SearchActivity} is expected to be
     *         launched.
     */
    static void assertSearchActivityLaunchedAfterAction(Runnable action) {
        Activity activity = ActivityTestUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), SearchActivity.class, action);
        Assert.assertNotNull(activity);
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
}
