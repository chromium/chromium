// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ui.messages.snackbar;

import android.app.Activity;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.AfterClass;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarController;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule.Component;

import java.io.IOException;
import java.util.List;

/** Render tests for {@link SnackbarView}. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@Batch(Batch.PER_CLASS)
public class SnackbarRenderTest {
    private static final int REVISION = 0;
    private static final int SNACKBAR_INTERVAL_MS = 100;
    private static final int SNACKBAR_MAX_TIMEOUT_MS = 10000;

    @ClassParameter
    private static final List<ParameterSet> sClassParams =
            new NightModeTestUtils.NightModeParams().getParameters();

    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(Component.UI_BROWSER_MOBILE)
                    .setRevision(REVISION)
                    .build();

    private Activity mActivity;
    private SnackbarView mSnackbarView;
    private SnackbarManager mSnackbarManager;
    private FrameLayout mParent;
    private ViewGroup mViewToRender;

    public SnackbarRenderTest(boolean nightModeEnabled) {
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @AfterClass
    public static void tearDownSuite() {
        NightModeTestUtils.tearDownNightModeForBlankUiTestActivity();
    }

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.launchActivity(null);
        mActivity = mActivityTestRule.getActivity();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mParent = mActivity.findViewById(android.R.id.content);
                    mSnackbarManager = new SnackbarManager(mActivity, mParent, null);
                    SnackbarManager.setDurationForTesting(10000);
                });
        CriteriaHelper.pollUiThread(
                () -> mSnackbarManager.canShowSnackbar(),
                "SnackBarManager should be ready to show the snackbar.");
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSnackbarManager.dismissAllSnackbars();
                });
        CriteriaHelper.pollUiThread(
                () -> !mSnackbarManager.isShowing(), "Snackbar should have been dismissed.");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @EnableFeatures(ChromeFeatureList.FLOATING_SNACKBAR)
    public void testFloatingSnackbarTypeAction() throws IOException {
        testFloatingSnackBar(Snackbar.TYPE_ACTION, "floating_snackbar_action");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @EnableFeatures(ChromeFeatureList.FLOATING_SNACKBAR)
    public void testFloatingSnackbarTypeNotification() throws IOException {
        testFloatingSnackBar(Snackbar.TYPE_NOTIFICATION, "floating_snackbar_notification");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @EnableFeatures(ChromeFeatureList.FLOATING_SNACKBAR)
    public void testFloatingSnackbarTypePersistent() throws IOException {
        testFloatingSnackBar(Snackbar.TYPE_PERSISTENT, "floating_snackbar_persistent");
    }

    public void testFloatingSnackBar(int type, String id) throws IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(
                            "SnackbarManger should be ready to show the snackbar.",
                            mSnackbarManager.canShowSnackbar());
                    Snackbar snackbar =
                            Snackbar.make(
                                    "Snackbar Render Test",
                                    new SnackbarController() {},
                                    type,
                                    Snackbar.UMA_TEST_SNACKBAR);
                    mSnackbarManager.showSnackbar(snackbar);
                });

        // Continuously poll from the UI thread until the view is showing.
        CriteriaHelper.pollUiThread(
                () ->
                        mSnackbarManager.getCurrentSnackbarViewForTesting() != null
                                && mSnackbarManager
                                                .getCurrentSnackbarViewForTesting()
                                                .getViewForTesting()
                                        != null,
                "Snackbar view should be fully visible.",
                SNACKBAR_MAX_TIMEOUT_MS,
                SNACKBAR_INTERVAL_MS);

        // Assign after pollUiThread to avoid null pointer exceptions.
        mSnackbarView = mSnackbarManager.getCurrentSnackbarViewForTesting();
        mViewToRender = mSnackbarView.getViewForTesting();
        mRenderTestRule.render(mViewToRender, id);
    }
}
