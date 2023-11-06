// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.animation.Animator;
import android.content.res.Resources;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.ColorInt;
import androidx.test.core.app.ActivityScenario;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.MathUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.ClipDrawableProgressBar.ProgressBarObserver;
import org.chromium.ui.base.TestActivity;

import java.util.concurrent.TimeoutException;

/** Tests related to the ToolbarProgressBar. */
@RunWith(BaseRobolectricTestRunner.class)
public class ToolbarProgressBarTest {
    @Mock ProgressBarObserver mMockProgressBarObserver;
    private ToolbarProgressBar mProgressBar;
    private ShadowLooper mShadowLooper;
    private ActivityScenario<TestActivity> mActivityScenario;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mShadowLooper = ShadowLooper.shadowMainLooper();

        mActivityScenario =
                ActivityScenario.launch(TestActivity.class)
                        .onActivity(
                                activity -> {
                                    activity.setTheme(R.style.Theme_BrowserUI_DayNight);

                                    ViewGroup view = new FrameLayout(activity);
                                    view.setVisibility(View.VISIBLE);
                                    FrameLayout.LayoutParams params =
                                            new FrameLayout.LayoutParams(
                                                    ViewGroup.LayoutParams.MATCH_PARENT,
                                                    ViewGroup.LayoutParams.MATCH_PARENT);
                                    activity.setContentView(view, params);

                                    Resources res = activity.getResources();
                                    int heightPx =
                                            res.getDimensionPixelSize(
                                                    R.dimen.toolbar_progress_bar_height);

                                    View anchor = new View(activity);
                                    view.addView(
                                            anchor,
                                            new FrameLayout.LayoutParams(
                                                    ViewGroup.LayoutParams.MATCH_PARENT,
                                                    heightPx * 2));

                                    mProgressBar =
                                            new ToolbarProgressBar(
                                                    activity, heightPx, anchor, false);
                                    final @ColorInt int toolbarColor =
                                            SemanticColorUtils.getToolbarBackgroundPrimary(
                                                    activity);
                                    mProgressBar.setThemeColor(toolbarColor, false);
                                    mProgressBar.setProgressBarObserver(mMockProgressBarObserver);

                                    view.addView(
                                            mProgressBar,
                                            new FrameLayout.LayoutParams(
                                                    ViewGroup.LayoutParams.MATCH_PARENT, heightPx));
                                });
    }

    @After
    public void tearDown() throws Exception {
        mActivityScenario.close();
    }

    /**
     * Get the current progress from the UI thread.
     *
     * @return The current progress displayed by the progress bar.
     */
    private float getProgress() {
        return mProgressBar.getProgress();
    }

    /**
     * Get the current progress bar visibility from the UI thread.
     *
     * @return The current progress displayed by the progress bar.
     */
    private boolean isProgressBarVisible() {
        return mProgressBar.getVisibility() == View.VISIBLE;
    }

    /** Test that the progress bar indeterminate animation completely traverses the screen. */
    @Test
    @Feature({"Android-Progress-Bar"})
    @SmallTest
    public void testProgressBarCompletion_indeterminateAnimation() throws TimeoutException {
        Animator progressAnimator = mProgressBar.getIndeterminateAnimatorForTesting();

        mProgressBar.start();
        assertFalse("Indeterminate animation should not be running.", progressAnimator.isRunning());

        // Wait for a visibility change.
        mShadowLooper.idle();
        verify(mMockProgressBarObserver, times(1)).onVisibilityChanged();

        mProgressBar.startIndeterminateAnimationForTesting();
        mProgressBar.setProgress(0.5f);

        assertTrue("Indeterminate animation should be running.", progressAnimator.isRunning());

        // Wait for progress updates to reach 50%.
        while (!MathUtils.areFloatsEqual(getProgress(), 0.5f)) {
            mShadowLooper.runOneTask();
        }

        mProgressBar.finish(true);

        // Wait for progress updates to reach 100%.
        while (!MathUtils.areFloatsEqual(getProgress(), 1.0f)) {
            mShadowLooper.runOneTask();
        }

        // Make sure the progress bar remains visible through completion.
        assertTrue("Progress bar should still be visible.", isProgressBarVisible());

        assertEquals("Progress should have reached 100%.", 1.0f, getProgress(), MathUtils.EPSILON);

        // Wait for a visibility change now that progress has completed.
        mShadowLooper.runToEndOfTasks();

        verify(mMockProgressBarObserver, times(2)).onVisibilityChanged();
        assertFalse("Indeterminate animation should not be running.", progressAnimator.isRunning());
        assertFalse("Progress bar should not be visible.", isProgressBarVisible());
    }

    /** Test that the progress bar completely traverses the screen without animation. */
    @Test
    @Feature({"Android-Progress-Bar"})
    @SmallTest
    public void testProgressBarCompletion_noAnimation() throws TimeoutException {
        mProgressBar.start();
        mProgressBar.setProgress(0.5f);

        // Wait for a visibility change.
        mShadowLooper.idle();
        verify(mMockProgressBarObserver).onVisibilityChanged();
        assertTrue("Progress bar should be visible.", isProgressBarVisible());

        // Ensure progress updates reached 50%.
        verify(mMockProgressBarObserver, times(1)).onVisibleProgressUpdated();
        assertEquals("Progress should have reached 50%.", 0.5f, getProgress(), MathUtils.EPSILON);

        // Finish progress bar.
        mProgressBar.finish(true);

        // Ensure progress reached 100%.
        verify(mMockProgressBarObserver, times(2)).onVisibleProgressUpdated();
        assertEquals("Progress should have reached 100%.", 1.0f, getProgress(), MathUtils.EPSILON);

        // Make sure the progress bar remains visible through completion.
        assertTrue("Progress bar should still be visible.", isProgressBarVisible());

        // Wait for hiding tasks.
        mShadowLooper.runToEndOfTasks();

        // Ensure that visibility changed now that progress has completed.
        assertFalse("Progress bar should not be visible.", isProgressBarVisible());
        verify(mMockProgressBarObserver, times(2)).onVisibilityChanged();
    }

    /** Test that the progress bar ends immediately if #finish(...) is called with delay = false. */
    @Test
    @Feature({"Android-Progress-Bar"})
    @SmallTest
    public void testProgressBarCompletion_indeterminateAnimation_noDelay() throws TimeoutException {
        Animator progressAnimator = mProgressBar.getIndeterminateAnimatorForTesting();

        mProgressBar.start();
        assertFalse("Indeterminate animation should not be running.", progressAnimator.isRunning());

        // Wait for a visibility change.
        mShadowLooper.idle();
        verify(mMockProgressBarObserver).onVisibilityChanged();
        assertTrue("Progress bar should be visible.", isProgressBarVisible());

        mProgressBar.startIndeterminateAnimationForTesting();
        mProgressBar.setProgress(0.5f);

        assertTrue("Indeterminate animation should be running.", progressAnimator.isRunning());

        // Wait for progress updates to reach 50%.
        while (!MathUtils.areFloatsEqual(getProgress(), 0.5f)) {
            mShadowLooper.runOneTask();
        }

        // Finish progress with no delay.
        mProgressBar.finish(false);

        // The progress bar should immediately be invisible.
        assertFalse("Progress bar should be invisible.", isProgressBarVisible());

        assertFalse("Indeterminate animation should not be running.", progressAnimator.isRunning());
    }

    /**
     * Test that the progress bar resets if a navigation occurs mid-progress while the indeterminate
     * animation is running.
     */
    @Test
    @Feature({"Android-Progress-Bar"})
    @SmallTest
    public void testProgressBarReset_indeterminateAnimation() throws TimeoutException {
        Animator progressAnimator = mProgressBar.getIndeterminateAnimatorForTesting();

        mProgressBar.start();
        assertFalse("Indeterminate animation should not be running.", progressAnimator.isRunning());
        // Wait for a visibility change.
        mShadowLooper.idle();
        verify(mMockProgressBarObserver).onVisibilityChanged();

        mProgressBar.startIndeterminateAnimationForTesting();
        mProgressBar.setProgress(0.5f);

        assertTrue("Indeterminate animation should be running.", progressAnimator.isRunning());

        // Wait for progress updates to reach 50%.
        while (!MathUtils.areFloatsEqual(getProgress(), 0.5f)) {
            mShadowLooper.runOneTask();
        }

        // Restart the progress bar.
        mProgressBar.start();

        // Cancel the indeterminate animator, it doesn't stop on its own.
        progressAnimator.cancel();

        // Make sure the progress bar remains visible through completion.
        assertTrue("Progress bar should still be visible.", isProgressBarVisible());

        assertEquals("Progress should be at 0%.", 0.0f, getProgress(), MathUtils.EPSILON);
    }
}
