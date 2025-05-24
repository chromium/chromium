// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.scrim;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
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
import androidx.core.graphics.ColorUtils;
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

    private ScrimManager mScrimManager;
    private View mAnchorView;

    private final PayloadCallbackHelper<Integer> mStatusBarColorHelper =
            new PayloadCallbackHelper<>();
    private final PayloadCallbackHelper<Integer> mNavBarColorHelper = new PayloadCallbackHelper<>();
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

                    mScrimManager = new ScrimManager(sActivity, sParent);
                    mScrimManager
                            .getStatusBarColorSupplier()
                            .addObserver(mStatusBarColorHelper::notifyCalled);
                    mScrimManager
                            .getNavigationBarColorSupplier()
                            .addObserver(mNavBarColorHelper::notifyCalled);

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
        // Wait for all the posted initial observations come back before test cases start.
        mStatusBarColorHelper.getOnlyPayloadBlocking();
        mNavBarColorHelper.getOnlyPayloadBlocking();
    }

    @After
    public void tearDownTest() {
        ThreadUtils.runOnUiThreadBlocking(() -> mScrimManager.destroy());
    }

    @Test
    @SmallTest
    @Feature({"Scrim"})
    public void testVisibility() throws TimeoutException {
        PropertyModel model = buildModel(false, true, Color.RED);
        showScrim(model, /* animate= */ false);

        assertEquals(
                "Scrim should be completely visible.",
                1.0f,
                mScrimManager.getViewForTesting().getAlpha(),
                MathUtils.EPSILON);

        int callCount = mVisibilityChangeCallbackHelper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> mScrimManager.hideScrim(model, /* animate= */ false));
        mVisibilityChangeCallbackHelper.waitForCallback(callCount, 1);
        assertScrimVisibility(false, model);
    }

    @Test
    @SmallTest
    @Feature({"Scrim"})
    public void testVisibilityWithForceToFinish() throws TimeoutException {
        PropertyModel model = buildModel(false, true, Color.RED);
        showScrim(model, /* animate= */ true);

        ScrimView scrimView = mScrimManager.getViewForTesting();
        assertEquals(
                "Scrim should be completely visible.",
                1.0f,
                scrimView.getAlpha(),
                MathUtils.EPSILON);

        int callCount = mVisibilityChangeCallbackHelper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mScrimManager.hideScrim(model, /* animate= */ true);
                    mScrimManager.forceAnimationToFinish(model);
                    assertEquals(
                            "Scrim should be completely invisible.",
                            0.0f,
                            scrimView.getAlpha(),
                            MathUtils.EPSILON);
                });
        mVisibilityChangeCallbackHelper.waitForCallback(callCount, 1);
        assertScrimVisibility(false, model);
    }

    @Test
    @SmallTest
    @Feature({"Scrim"})
    public void testColor_default() throws TimeoutException {
        showScrim(buildModel(false, true, Color.RED), /* animate= */ false);

        assertScrimColor(Color.RED);
    }

    @Test
    @SmallTest
    @Feature({"Scrim"})
    public void testColor_custom() throws TimeoutException {
        PropertyModel model = buildModel(false, true, Color.GREEN);
        showScrim(model, /* animate= */ false);

        assertScrimColor(Color.GREEN);

        ThreadUtils.runOnUiThreadBlocking(
                () -> mScrimManager.hideScrim(model, /* animate= */ false));

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "Scrim should be null after being hidden.",
                            mScrimManager.getViewForTesting(),
                            Matchers.nullValue());
                });
    }

    @Test
    @SmallTest
    @Feature({"Scrim"})
    public void testColor_mutated() throws TimeoutException {
        PropertyModel model = buildModel(true, true, Color.GREEN);

        int callCount = mStatusBarColorHelper.getCallCount();
        showScrim(model, /* animate= */ false);
        assertScrimColor(Color.GREEN);
        assertEquals(
                Color.GREEN, mStatusBarColorHelper.getPayloadByIndexBlocking(callCount).intValue());

        callCount = mStatusBarColorHelper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(() -> mScrimManager.setScrimColor(Color.RED, model));
        assertScrimColor(Color.RED);
        assertEquals(
                Color.RED, mStatusBarColorHelper.getPayloadByIndexBlocking(callCount).intValue());
    }

    @Test
    @SmallTest
    @Feature({"Scrim"})
    public void testHierarchy_behindAnchor() throws TimeoutException {
        showScrim(buildModel(false, false, Color.RED), /* animate= */ false);

        View scrimView = mScrimManager.getViewForTesting();
        assertEquals("The parent view of the scrim is incorrect.", sParent, scrimView.getParent());
        assertTrue(
                "The scrim should be positioned behind the anchor.",
                sParent.indexOfChild(scrimView) < sParent.indexOfChild(mAnchorView));
    }

    @Test
    @SmallTest
    @Feature({"Scrim"})
    public void testHierarchy_inFrontOfAnchor() throws TimeoutException {
        showScrim(buildModel(false, true, Color.RED), /* animate= */ false);

        View scrimView = mScrimManager.getViewForTesting();
        assertEquals("The parent view of the scrim is incorrect.", sParent, scrimView.getParent());
        assertTrue(
                "The scrim should be positioned in front of the anchor.",
                sParent.indexOfChild(scrimView) > sParent.indexOfChild(mAnchorView));
    }

    @Test
    @SmallTest
    @Feature({"Scrim"})
    public void testObserver_clickEvent() throws TimeoutException {
        showScrim(buildModel(false, true, Color.RED), /* animate= */ false);

        int callCount = mScrimClickCallbackHelper.getCallCount();
        ScrimView scrimView = mScrimManager.getViewForTesting();
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

        showScrim(model, /* animate= */ false);

        int gestureCallCount = mDelegatedEventHelper.getCallCount();
        ScrimView scrimView = mScrimManager.getViewForTesting();
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
        showScrim(buildModel(false, true, Color.RED), /* animate= */ true);
    }

    @Test
    @SmallTest
    @Feature({"Scrim"})
    public void testAnimation_canceled() throws TimeoutException {
        PropertyModel model = buildModel(false, true, Color.RED);
        showScrim(model, /* animate= */ true);

        ThreadUtils.runOnUiThreadBlocking(() -> mScrimManager.setAlpha(0.5f, model));

        assertFalse(
                "Animations should not be running.",
                mScrimManager.areAnimationsRunningForTesting(model));
    }

    @Test
    @SmallTest
    @Feature({"Scrim"})
    public void testAffectsStatusBar_enabled() throws TimeoutException {
        int colorCallCount = mStatusBarColorHelper.getCallCount();
        showScrim(buildModel(true, true, Color.RED), /* animate= */ false);
        assertEquals(
                Color.RED,
                mStatusBarColorHelper.getPayloadByIndexBlocking(colorCallCount).intValue());
    }

    @Test
    @SmallTest
    @Feature({"Scrim"})
    public void testAffectsStatusBar_disabled() throws TimeoutException {
        int callCount = mStatusBarColorHelper.getCallCount();
        PropertyModel model = buildModel(false, true, Color.RED);
        showScrim(model, /* animate= */ false);

        ThreadUtils.runOnUiThreadBlocking(() -> mScrimManager.setAlpha(0.5f, model));

        assertEquals(
                "Scrim alpha should be 0.5f.",
                0.5f,
                mScrimManager.getViewForTesting().getAlpha(),
                MathUtils.EPSILON);

        assertEquals(
                "No events to the status bar color callback should have occurred",
                callCount,
                mStatusBarColorHelper.getCallCount());
    }

    @Test
    @SmallTest
    @Feature({"Scrim"})
    public void testAffectsNavigationBar_enabled() throws TimeoutException {
        int colorCallCount = mNavBarColorHelper.getCallCount();
        PropertyModel model =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return new PropertyModel.Builder(ALL_KEYS)
                                    .with(ANCHOR_VIEW, mAnchorView)
                                    .with(CLICK_DELEGATE, mClickDelegate)
                                    .with(VISIBILITY_CALLBACK, mVisibilityChangeCallback)
                                    .with(AFFECTS_NAVIGATION_BAR, true)
                                    .with(BACKGROUND_COLOR, Color.RED)
                                    .build();
                        });
        showScrim(model, /* animate= */ false);

        assertEquals(
                Color.RED, mNavBarColorHelper.getPayloadByIndexBlocking(colorCallCount).intValue());
    }

    @Test
    @SmallTest
    @Feature({"Scrim"})
    public void testAffectsNavigationBar_disabled() throws TimeoutException {
        int callCount = mNavBarColorHelper.getCallCount();
        PropertyModel model =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return new PropertyModel.Builder(ALL_KEYS)
                                    .with(ANCHOR_VIEW, mAnchorView)
                                    .with(CLICK_DELEGATE, mClickDelegate)
                                    .with(VISIBILITY_CALLBACK, mVisibilityChangeCallback)
                                    .build();
                        });
        showScrim(model, /* animate= */ false);

        assertEquals(
                "No events to the navigation bar callback should have occurred",
                callCount,
                mNavBarColorHelper.getCallCount());
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

        showScrim(model, /* animate= */ false);

        View scrimView = mScrimManager.getViewForTesting();
        assertEquals(
                "Scrim top margin is incorrect.",
                topMargin,
                ((ViewGroup.MarginLayoutParams) scrimView.getLayoutParams()).topMargin);
    }

    @Test
    @SmallTest
    @Feature({"Scrim"})
    public void testScrimVisibilityObserver() throws TimeoutException {
        class TestScrimVisibilityObserver implements Observer {
            private boolean mCurrentVisible;

            @Override
            public void scrimVisibilityChanged(boolean scrimVisible) {
                mCurrentVisible = scrimVisible;
            }

            public void assertVisibility(boolean expectedVisible) {
                assertEquals(expectedVisible, mCurrentVisible);
            }
        }
        TestScrimVisibilityObserver o1 = new TestScrimVisibilityObserver();

        ThreadUtils.runOnUiThreadBlocking(() -> mScrimManager.addObserver(o1));
        PropertyModel firstModel = buildModel(false, true, Color.RED);
        showScrim(firstModel, /* animate= */ false);
        o1.assertVisibility(true);

        PropertyModel secondModel = buildModel(false, true, Color.BLUE);
        showScrim(secondModel, /* animate= */ false);
        o1.assertVisibility(true);

        ThreadUtils.runOnUiThreadBlocking(
                () -> mScrimManager.hideScrim(firstModel, /* animate= */ false));
        // Above hideScrim should no-op, wrong model.
        o1.assertVisibility(true);

        ThreadUtils.runOnUiThreadBlocking(
                () -> mScrimManager.hideScrim(secondModel, /* animate= */ false));
        o1.assertVisibility(false);

        showScrim(buildModel(false, true, Color.BLUE), /* animate= */ false);
        o1.assertVisibility(true);
    }

    @Test
    @SmallTest
    @Feature({"Scrim"})
    public void testStackedScrims() {
        mScrimManager.disableAnimationForTesting(true);

        PropertyModel model1 =
                buildModel(
                        /* affectsStatusBar= */ true, /* showInFrontOfAnchor= */ false, Color.RED);
        PropertyModel model2 =
                buildModel(
                        /* affectsStatusBar= */ true, /* showInFrontOfAnchor= */ false, Color.BLUE);
        PropertyModel model3 =
                buildModel(
                        /* affectsStatusBar= */ true,
                        /* showInFrontOfAnchor= */ false,
                        Color.GREEN);
        @ColorInt int color4 = ColorUtils.setAlphaComponent(Color.GREEN, 128);
        PropertyModel model4 =
                buildModel(/* affectsStatusBar= */ true, /* showInFrontOfAnchor= */ false, color4);

        assertStatusBarColor(Color.TRANSPARENT);

        ThreadUtils.runOnUiThreadBlocking(() -> mScrimManager.showScrim(model1));
        assertStatusBarColor(Color.RED);

        ThreadUtils.runOnUiThreadBlocking(() -> mScrimManager.showScrim(model2));
        assertStatusBarColor(Color.BLUE);

        ThreadUtils.runOnUiThreadBlocking(() -> mScrimManager.showScrim(model3));
        assertStatusBarColor(Color.GREEN);

        ThreadUtils.runOnUiThreadBlocking(
                () -> mScrimManager.hideScrim(model2, /* animate= */ false));
        assertStatusBarColor(Color.GREEN);

        ThreadUtils.runOnUiThreadBlocking(
                () -> mScrimManager.hideScrim(model3, /* animate= */ false));
        assertStatusBarColor(Color.RED);

        ThreadUtils.runOnUiThreadBlocking(() -> mScrimManager.showScrim(model4));
        assertStatusBarColor(ColorUtils.compositeColors(color4, Color.RED));
    }

    /**
     * Build a model to show the scrim with.
     *
     * @param affectsStatusBar Whether the status bar should be affected by the scrim.
     * @param showInFrontOfAnchor Whether the scrim shows in front of the anchor view.
     * @param color The color to use for the overlay. If only using required keys, this value is
     *     ignored.
     * @return A model to pass to the {@link ScrimManager}.
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
                    mScrimManager.showScrim(model);

                    // Animations are disabled for these types of tests, so just make sure the
                    // animation was created then continue as if we weren't running animation.
                    if (animate) {
                        assertTrue(
                                "Animations should be running.",
                                mScrimManager.areAnimationsRunningForTesting(model));
                    }

                    mScrimManager.forceAnimationToFinish(model);
                    assertFalse(mScrimManager.areAnimationsRunningForTesting(model));
                    assertEquals(
                            "Scrim should be completely visible.",
                            1.0f,
                            mScrimManager.getViewForTesting(model).getAlpha(),
                            MathUtils.EPSILON);
                });

        mVisibilityChangeCallbackHelper.waitForCallback(callCount, 1);
        assertScrimVisibility(true, model);
    }

    /** Assert that the scrim background is a specific color. */
    private void assertScrimColor(@ColorInt int color) {
        assertEquals(
                "Scrim color was incorrect.",
                color,
                ((ColorDrawable) mScrimManager.getViewForTesting().getBackground()).getColor());
    }

    /** Assert that the scrim background is a specific color. */
    private void assertStatusBarColor(@ColorInt int color) {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(
                            mScrimManager.getStatusBarColorSupplier().get().intValue(),
                            Matchers.is(color));
                });
    }

    /**
     * Assert that the scrim is the desired visibility.
     *
     * @param visible Whether the scrim should be visible.
     * @param model The model used to show the scrim.
     */
    private void assertScrimVisibility(boolean visible, PropertyModel model) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    if (visible) {
                        assertEquals(
                                "The scrim should be visible.",
                                View.VISIBLE,
                                mScrimManager.getViewForTesting(model).getVisibility());
                    } else {
                        assertNull(
                                "The scrim should be null after being hidden.",
                                mScrimManager.getViewForTesting(model));
                    }
                });
    }
}
