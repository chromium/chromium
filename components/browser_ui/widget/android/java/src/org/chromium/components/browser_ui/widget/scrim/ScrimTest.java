// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.scrim;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.components.browser_ui.widget.scrim.ScrimProperties.AFFECTS_NAVIGATION_BAR;
import static org.chromium.components.browser_ui.widget.scrim.ScrimProperties.AFFECTS_STATUS_BAR;
import static org.chromium.components.browser_ui.widget.scrim.ScrimProperties.ALL_KEYS;
import static org.chromium.components.browser_ui.widget.scrim.ScrimProperties.ANCHOR_VIEW;
import static org.chromium.components.browser_ui.widget.scrim.ScrimProperties.BACKGROUND_COLOR;
import static org.chromium.components.browser_ui.widget.scrim.ScrimProperties.CLICK_DELEGATE;
import static org.chromium.components.browser_ui.widget.scrim.ScrimProperties.GESTURE_DETECTOR;
import static org.chromium.components.browser_ui.widget.scrim.ScrimProperties.SHOW_IN_FRONT_OF_ANCHOR_VIEW;
import static org.chromium.components.browser_ui.widget.scrim.ScrimProperties.TOP_MARGIN;
import static org.chromium.components.browser_ui.widget.scrim.ScrimProperties.VISIBILITY_CALLBACK;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.MathUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.PayloadCallbackHelper;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator.Observer;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.BlankUiTestActivity;

import java.util.concurrent.TimeoutException;

/** This class tests the behavior of the scrim component. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class ScrimTest {
    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> activityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @SuppressLint("StaticFieldLeak")
    private static Activity sActivity;

    @SuppressLint("StaticFieldLeak")
    private static FrameLayout sParent;

    private ScrimCoordinator mScrimCoordinator;
    private View mAnchorView;

    private final PayloadCallbackHelper<Integer> mScrimColorCallbackHelper =
            new PayloadCallbackHelper<>();
    private final CallbackHelper mStatusBarCallbackHelper = new CallbackHelper();
    private final CallbackHelper mNavigationBarCallbackHelper = new CallbackHelper();
    private final ScrimCoordinator.SystemUiScrimDelegate mScrimDelegate =
            new ScrimCoordinator.SystemUiScrimDelegate() {
                @Override
                public void setScrimColor(@ColorInt int scrimColor) {
                    mScrimColorCallbackHelper.notifyCalled(scrimColor);
                }

                @Override
                public void setStatusBarScrimFraction(float scrimFraction) {
                    mStatusBarCallbackHelper.notifyCalled();
                }

                @Override
                public void setNavigationBarScrimFraction(float scrimFraction) {
                    mNavigationBarCallbackHelper.notifyCalled();
                }
            };

    private final CallbackHelper mScrimClickCallbackHelper = new CallbackHelper();
    private final CallbackHelper mVisibilityChangeCallbackHelper = new CallbackHelper();
    private final Runnable mClickDelegate = mScrimClickCallbackHelper::notifyCalled;
    private final Callback<Boolean> mVisibilityChangeCallback =
            (v) -> mVisibilityChangeCallbackHelper.notifyCalled();

    private GestureDetector mCustomGestureDetector;
    private CallbackHelper mDelegatedEventHelper;

    @BeforeClass
    public static void setupSuite() {
        activityTestRule.launchActivity(null);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sActivity = activityTestRule.getActivity();
                    sParent = new FrameLayout(sActivity);
                    sActivity.setContentView(sParent);
                });
    }

    @Before
    public void setupTest() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sParent.removeAllViews();

                    mAnchorView = new View(sActivity);
                    sParent.addView(mAnchorView);

                    mScrimCoordinator = new ScrimCoordinator(sActivity, mScrimDelegate, sParent);

                    mDelegatedEventHelper = new CallbackHelper();
                    mCustomGestureDetector =
                            new GestureDetector(
                                    new GestureDetector.SimpleOnGestureListener() {
                                        @Override
                                        public boolean onDown(@NonNull MotionEvent e) {
                                            mDelegatedEventHelper.notifyCalled();
                                            return true;
                                        }
                                    });
                });
    }

    @After
    public void tearDownTest() {
        ThreadUtils.runOnUiThreadBlocking(() -> mScrimCoordinator.destroy());
    }

    @Test
    @SmallTest
    @Feature({"Scrim"})
    public void testVisibility() throws TimeoutException {
        showScrim(buildModel(false, true, Color.RED), false);

        assertEquals(
                "Scrim should be completely visible.",
                1.0f,
                mScrimCoordinator.getViewForTesting().getAlpha(),
                MathUtils.EPSILON);

        int callCount = mVisibilityChangeCallbackHelper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(() -> mScrimCoordinator.hideScrim(false));
        mVisibilityChangeCallbackHelper.waitForCallback(callCount, 1);
        assertScrimVisibility(false);
    }

    @Test
    @SmallTest
    @Feature({"Scrim"})
    public void testVisibilityWithForceToFinish() throws TimeoutException {
        showScrim(buildModel(false, true, Color.RED), true);

        ScrimView scrimView = mScrimCoordinator.getViewForTesting();
        assertEquals(
                "Scrim should be completely visible.",
                1.0f,
                scrimView.getAlpha(),
                MathUtils.EPSILON);

        int callCount = mVisibilityChangeCallbackHelper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mScrimCoordinator.hideScrim(true);
                    mScrimCoordinator.forceAnimationToFinish();
                    assertEquals(
                            "Scrim should be completely invisible.",
                            0.0f,
                            scrimView.getAlpha(),
                            MathUtils.EPSILON);
                });
        mVisibilityChangeCallbackHelper.waitForCallback(callCount, 1);
        assertScrimVisibility(false);
    }

    @Test
    @SmallTest
    @Feature({"Scrim"})
    public void testColor_default() throws TimeoutException {
        showScrim(buildModel(false, true, Color.RED), false);

        assertScrimColor(Color.RED);
    }

    @Test
    @SmallTest
    @Feature({"Scrim"})
    public void testColor_custom() throws TimeoutException {
        showScrim(buildModel(false, true, Color.GREEN), false);

        assertScrimColor(Color.GREEN);

        ThreadUtils.runOnUiThreadBlocking(() -> mScrimCoordinator.hideScrim(false));

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "Scrim should be null after being hidden.",
                            mScrimCoordinator.getViewForTesting(),
                            Matchers.nullValue());
                });
    }

    @Test
    @SmallTest
    @Feature({"Scrim"})
    public void testColor_mutated() throws TimeoutException {
        PropertyModel model = buildModel(false, true, Color.GREEN);

        showScrim(model, false);
        assertScrimColor(Color.GREEN);
        assertEquals(Color.GREEN, mScrimColorCallbackHelper.getOnlyPayloadBlocking().intValue());

        ThreadUtils.runOnUiThreadBlocking(() -> mScrimCoordinator.setScrimColor(Color.RED, model));
        assertScrimColor(Color.RED);
        assertEquals(Color.RED, mScrimColorCallbackHelper.getPayloadByIndexBlocking(1).intValue());
    }

    @Test
    @SmallTest
    @Feature({"Scrim"})
    public void testHierarchy_behindAnchor() throws TimeoutException {
        showScrim(buildModel(false, false, Color.RED), false);

        View scrimView = mScrimCoordinator.getViewForTesting();
        assertEquals("The parent view of the scrim is incorrect.", sParent, scrimView.getParent());
        assertTrue(
                "The scrim should be positioned behind the anchor.",
                sParent.indexOfChild(scrimView) < sParent.indexOfChild(mAnchorView));
    }

    @Test
    @SmallTest
    @Feature({"Scrim"})
    public void testHierarchy_inFrontOfAnchor() throws TimeoutException {
        showScrim(buildModel(false, true, Color.RED), false);

        View scrimView = mScrimCoordinator.getViewForTesting();
        assertEquals("The parent view of the scrim is incorrect.", sParent, scrimView.getParent());
        assertTrue(
                "The scrim should be positioned in front of the anchor.",
                sParent.indexOfChild(scrimView) > sParent.indexOfChild(mAnchorView));
    }

    @Test
    @SmallTest
    @Feature({"Scrim"})
    public void testObserver_clickEvent() throws TimeoutException {
        showScrim(buildModel(false, true, Color.RED), false);

        int callCount = mScrimClickCallbackHelper.getCallCount();
        ScrimView scrimView = mScrimCoordinator.getViewForTesting();
        ThreadUtils.runOnUiThreadBlocking(scrimView::callOnClick);
        mScrimClickCallbackHelper.waitForCallback(callCount, 1);
    }

    @Test
    @SmallTest
    @Feature({"Scrim"})
    public void testGestureDetector() throws TimeoutException {
        PropertyModel model =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return new PropertyModel.Builder(ALL_KEYS)
                                    .with(ANCHOR_VIEW, mAnchorView)
                                    .with(CLICK_DELEGATE, mClickDelegate)
                                    .with(VISIBILITY_CALLBACK, mVisibilityChangeCallback)
                                    .with(BACKGROUND_COLOR, Color.RED)
                                    .with(GESTURE_DETECTOR, mCustomGestureDetector)
                                    .build();
                        });

        showScrim(model, false);

        int gestureCallCount = mDelegatedEventHelper.getCallCount();
        ScrimView scrimView = mScrimCoordinator.getViewForTesting();
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        scrimView.dispatchTouchEvent(
                                MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, 0, 0)));
        mDelegatedEventHelper.waitForCallback(gestureCallCount, 1);
    }

    @Test
    @SmallTest
    @Feature({"Scrim"})
    public void testAnimation_running() throws TimeoutException {
        // The showScrim method includes checks for animation state.
        showScrim(buildModel(false, true, Color.RED), true);
    }

    @Test
    @SmallTest
    @Feature({"Scrim"})
    public void testAnimation_canceled() throws TimeoutException {
        PropertyModel model = buildModel(false, true, Color.RED);
        showScrim(model, true);

        ThreadUtils.runOnUiThreadBlocking(() -> mScrimCoordinator.setAlpha(0.5f, model));

        assertFalse(
                "Animations should not be running.",
                mScrimCoordinator.areAnimationsRunningForTesting());
    }

    @Test
    @SmallTest
    @Feature({"Scrim"})
    public void testAffectsStatusBar_enabled() throws TimeoutException {
        int callCount = mStatusBarCallbackHelper.getCallCount();
        showScrim(buildModel(true, true, Color.RED), false);
        mStatusBarCallbackHelper.waitForCallback(callCount, 1);
    }

    @Test
    @SmallTest
    @Feature({"Scrim"})
    public void testAffectsStatusBar_disabled() throws TimeoutException {
        int callCount = mStatusBarCallbackHelper.getCallCount();
        PropertyModel model = buildModel(false, true, Color.RED);
        showScrim(model, false);

        ThreadUtils.runOnUiThreadBlocking(() -> mScrimCoordinator.setAlpha(0.5f, model));

        assertEquals(
                "Scrim alpha should be 0.5f.",
                0.5f,
                mScrimCoordinator.getViewForTesting().getAlpha(),
                MathUtils.EPSILON);

        assertEquals(
                "No events to the status bar delegate should have occurred",
                callCount,
                mStatusBarCallbackHelper.getCallCount());
    }

    @Test
    @SmallTest
    @Feature({"Scrim"})
    public void testAffectsNavigationBar_enabled() throws TimeoutException {
        int callCount = mNavigationBarCallbackHelper.getCallCount();
        PropertyModel model =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return new PropertyModel.Builder(ALL_KEYS)
                                    .with(ANCHOR_VIEW, mAnchorView)
                                    .with(CLICK_DELEGATE, mClickDelegate)
                                    .with(VISIBILITY_CALLBACK, mVisibilityChangeCallback)
                                    .with(AFFECTS_NAVIGATION_BAR, true)
                                    .build();
                        });
        showScrim(model, false);

        mNavigationBarCallbackHelper.waitForCallback(callCount, 1);
    }

    @Test
    @SmallTest
    @Feature({"Scrim"})
    public void testAffectsNavigationBar_disabled() throws TimeoutException {
        int callCount = mStatusBarCallbackHelper.getCallCount();
        PropertyModel model =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return new PropertyModel.Builder(ALL_KEYS)
                                    .with(ANCHOR_VIEW, mAnchorView)
                                    .with(CLICK_DELEGATE, mClickDelegate)
                                    .with(VISIBILITY_CALLBACK, mVisibilityChangeCallback)
                                    .build();
                        });
        showScrim(model, false);

        assertEquals(
                "No events to the navigation bar delegate should have occurred",
                callCount,
                mNavigationBarCallbackHelper.getCallCount());
    }

    @Test
    @SmallTest
    @Feature({"Scrim"})
    public void testTopMargin() throws TimeoutException {
        int topMargin = 100;
        PropertyModel model =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return new PropertyModel.Builder(ALL_KEYS)
                                    .with(TOP_MARGIN, topMargin)
                                    .with(ANCHOR_VIEW, mAnchorView)
                                    .with(CLICK_DELEGATE, mClickDelegate)
                                    .with(VISIBILITY_CALLBACK, mVisibilityChangeCallback)
                                    .build();
                        });

        showScrim(model, false);

        View scrimView = mScrimCoordinator.getViewForTesting();
        assertEquals(
                "Scrim top margin is incorrect.",
                topMargin,
                ((ViewGroup.MarginLayoutParams) scrimView.getLayoutParams()).topMargin);
    }

    @Test
    @SmallTest
    @Feature({"Scrim"})
    public void testOldScrimHidden() throws TimeoutException {
        PropertyModel firstModel = buildModel(false, true, Color.RED);
        showScrim(firstModel, false);

        assertScrimVisibility(true);

        View oldScrim = mScrimCoordinator.getViewForTesting();

        showScrim(buildModel(false, true, Color.BLUE), false);
        assertScrimColor(Color.BLUE);

        View newScrim = mScrimCoordinator.getViewForTesting();

        assertNotEquals("The view should have changed.", oldScrim, newScrim);
        assertEquals("The old scrim should be gone.", View.GONE, oldScrim.getVisibility());

        ThreadUtils.runOnUiThreadBlocking(() -> firstModel.set(BACKGROUND_COLOR, Color.MAGENTA));
        assertScrimColor(Color.BLUE);

        ThreadUtils.runOnUiThreadBlocking(() -> mScrimCoordinator.hideScrim(false));
        ThreadUtils.runOnUiThreadBlocking(() -> firstModel.set(BACKGROUND_COLOR, Color.GREEN));
    }

    @Test
    @SmallTest
    @Feature({"Scrim"})
    public void testScrimVisibilityObserver() throws TimeoutException {
        class TestScrimVisibilityObserver implements Observer {
            public boolean mVisible;

            @Override
            public void scrimVisibilityChanged(boolean scrimVisible) {
                mVisible = scrimVisible;
            }
        }
        TestScrimVisibilityObserver o1 = new TestScrimVisibilityObserver();
        TestScrimVisibilityObserver o2 = new TestScrimVisibilityObserver();

        ThreadUtils.runOnUiThreadBlocking(() -> mScrimCoordinator.addObserver(o1));
        PropertyModel firstModel = buildModel(false, true, Color.RED);
        showScrim(firstModel, false);

        assertTrue(o1.mVisible);
        assertFalse(o2.mVisible);

        ThreadUtils.runOnUiThreadBlocking(() -> mScrimCoordinator.addObserver(o2));

        showScrim(buildModel(false, true, Color.BLUE), false);

        assertTrue(o1.mVisible);
        // No update for o2 yet since the visibility hasn't changed.
        assertFalse(o2.mVisible);

        ThreadUtils.runOnUiThreadBlocking(() -> mScrimCoordinator.hideScrim(false));
        assertFalse(o1.mVisible);
        assertFalse(o2.mVisible);

        showScrim(buildModel(false, true, Color.BLUE), false);

        assertTrue(o1.mVisible);
        assertTrue(o2.mVisible);
    }

    /**
     * Build a model to show the scrim with.
     *
     * @param affectsStatusBar Whether the status bar should be affected by the scrim.
     * @param showInFrontOfAnchor Whether the scrim shows in front of the anchor view.
     * @param color The color to use for the overlay. If only using required keys, this value is
     *     ignored.
     * @return A model to pass to the scrim coordinator.
     */
    private PropertyModel buildModel(
            boolean affectsStatusBar, boolean showInFrontOfAnchor, @ColorInt int color) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    return new PropertyModel.Builder(ALL_KEYS)
                            .with(AFFECTS_STATUS_BAR, affectsStatusBar)
                            .with(ANCHOR_VIEW, mAnchorView)
                            .with(SHOW_IN_FRONT_OF_ANCHOR_VIEW, showInFrontOfAnchor)
                            .with(CLICK_DELEGATE, mClickDelegate)
                            .with(VISIBILITY_CALLBACK, mVisibilityChangeCallback)
                            .with(BACKGROUND_COLOR, color)
                            .build();
                });
    }

    /**
     * Show the scrim and wait for a visibility change.
     *
     * @param model The model to show the scrim with.
     * @param animate Whether the scrim should animate. If false, alpha is immediately set to 100%.
     */
    private void showScrim(PropertyModel model, boolean animate) throws TimeoutException {
        int callCount = mVisibilityChangeCallbackHelper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mScrimCoordinator.showScrim(model);

                    // Animations are disabled for these types of tests, so just make sure the
                    // animation was created then continue as if we weren't running animation.
                    if (animate) {
                        assertTrue(
                                "Animations should be running.",
                                mScrimCoordinator.areAnimationsRunningForTesting());
                    }

                    mScrimCoordinator.forceAnimationToFinish();
                    assertFalse(mScrimCoordinator.areAnimationsRunningForTesting());
                    assertEquals(
                            "Scrim should be completely visible.",
                            1.0f,
                            mScrimCoordinator.getViewForTesting().getAlpha(),
                            MathUtils.EPSILON);
                });

        mVisibilityChangeCallbackHelper.waitForCallback(callCount, 1);
        assertScrimVisibility(true);
    }

    /** Assert that the scrim background is a specific color. */
    private void assertScrimColor(@ColorInt int color) {
        assertEquals(
                "Scrim color was incorrect.",
                color,
                ((ColorDrawable) mScrimCoordinator.getViewForTesting().getBackground()).getColor());
    }

    /**
     * Assert that the scrim is the desired visibility.
     *
     * @param visible Whether the scrim should be visible.
     */
    private void assertScrimVisibility(final boolean visible) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    if (visible) {
                        assertEquals(
                                "The scrim should be visible.",
                                View.VISIBLE,
                                mScrimCoordinator.getViewForTesting().getVisibility());
                    } else {
                        assertNull(
                                "The scrim should be null after being hidden.",
                                mScrimCoordinator.getViewForTesting());
                    }
                });
    }
}
