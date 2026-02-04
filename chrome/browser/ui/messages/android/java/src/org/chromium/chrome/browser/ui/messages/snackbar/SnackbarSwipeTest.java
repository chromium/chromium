// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.messages.snackbar;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarController;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.ui.test.util.BlankUiTestActivity;

/** Tests for snackbar swipe to dismiss. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class SnackbarSwipeTest {
    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private SnackbarManager mManager;
    private Activity mActivity;
    private FrameLayout mParent;
    private boolean mDismissed;

    private final SnackbarController mDefaultController =
            new SnackbarController() {
                @Override
                public void onDismissNoAction(Object actionData) {
                    mDismissed = true;
                }
            };

    @Before
    public void setUp() {
        mActivityTestRule.launchActivity(null);
        mActivity = mActivityTestRule.getActivity();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mParent = mActivity.findViewById(android.R.id.content);
                    mManager = new SnackbarManager(mActivity, mParent, null);
                    SnackbarManager.setDurationForTesting(10000);
                });
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mManager.dismissAllSnackbars();
                    SnackbarManager.resetDurationForTesting();
                });
    }

    @Test
    @MediumTest
    public void testSwipeToDismiss() {
        Snackbar snackbar =
                Snackbar.make(
                        "Swipe to dismiss",
                        mDefaultController,
                        Snackbar.TYPE_ACTION,
                        Snackbar.UMA_TEST_SNACKBAR);
        ThreadUtils.runOnUiThreadBlocking(() -> mManager.showSnackbar(snackbar));

        CriteriaHelper.pollUiThread(() -> mManager.isShowing(), "Snackbar should be showing");

        SnackbarView snackbarView = mManager.getCurrentSnackbarViewForTesting();
        ViewGroup container = snackbarView.mContainerView;
        CriteriaHelper.pollUiThread(() -> container.getWidth() > 0, "Container not laid out");

        // Wait for the show animation to finish.
        CriteriaHelper.pollUiThread(
                () -> container.getTranslationY() == 0f, "Snackbar animation did not finish.");

        int[] location = new int[2];
        container.getLocationOnScreen(location);
        int width = container.getWidth();
        int height = container.getHeight();

        float startX = location[0] + width / 2f;
        float startY = location[1] + height / 2f;
        float endX = startX + width; // Swipe to the right
        float endY = startY;

        mDismissed = false;
        TouchCommon.performWallClockDrag(mActivity, startX, endX, startY, endY, 2000, 60, true);
        CriteriaHelper.pollUiThread(() -> !mManager.isShowing(), "Snackbar should be dismissed");
        assertTrue("onDismissNoAction should be called", mDismissed);
    }

    @Test
    @MediumTest
    public void testSwipeInsufficientDistance() {
        Snackbar snackbar =
                Snackbar.make(
                        "Swipe insufficient",
                        mDefaultController,
                        Snackbar.TYPE_ACTION,
                        Snackbar.UMA_TEST_SNACKBAR);
        ThreadUtils.runOnUiThreadBlocking(() -> mManager.showSnackbar(snackbar));

        CriteriaHelper.pollUiThread(() -> mManager.isShowing(), "Snackbar should be showing");

        SnackbarView snackbarView = mManager.getCurrentSnackbarViewForTesting();
        ViewGroup container = (ViewGroup) snackbarView.getViewForTesting().getParent();
        CriteriaHelper.pollUiThread(() -> container.getWidth() > 0, "Container not laid out");

        // Wait for the show animation to finish.
        CriteriaHelper.pollUiThread(
                () -> container.getTranslationY() == 0f, "Snackbar animation did not finish.");

        int[] location = new int[2];
        container.getLocationOnScreen(location);
        int width = container.getWidth();
        int height = container.getHeight();

        float startX = location[0] + width / 2f;
        float startY = location[1] + height / 2f;
        float endX = startX + 5; // Very small swipe
        float endY = startY;

        mDismissed = false;
        TouchCommon.performWallClockDrag(mActivity, startX, endX, startY, endY, 200, 20, true);

        // Snackbar should still be showing
        assertFalse("Snackbar should not be dismissed", mDismissed);
        assertTrue("Snackbar should still be showing", mManager.isShowing());
    }
}
