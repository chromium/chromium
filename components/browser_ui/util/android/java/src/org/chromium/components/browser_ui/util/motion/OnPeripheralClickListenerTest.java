// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.util.motion;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.os.SystemClock;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.widget.TextView;

import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.util.motion.OnPeripheralClickListener.OnPeripheralClickRunnable;
import org.chromium.ui.test.util.BlankUiTestActivity;

@NullMarked
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class OnPeripheralClickListenerTest {

    @Rule
    public final BaseActivityTestRule<BlankUiTestActivity> mActivityRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Test
    @SmallTest
    public void testClickFromTouchScreen() {
        View testView = setUpTestView();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Setup
                    TestOnPeripheralClickRunnable onPeripheralClickRunnable =
                            new TestOnPeripheralClickRunnable();
                    testView.setOnTouchListener(
                            new OnPeripheralClickListener(testView, onPeripheralClickRunnable));

                    // Act
                    long downMotionTime = SystemClock.uptimeMillis();
                    boolean downMotionHandled =
                            testView.dispatchTouchEvent(
                                    MotionEventTestUtils.createTouchMotionEvent(
                                            downMotionTime,
                                            /* eventTime= */ downMotionTime,
                                            MotionEvent.ACTION_DOWN));
                    boolean upMotionHandled =
                            testView.dispatchTouchEvent(
                                    MotionEventTestUtils.createTouchMotionEvent(
                                            downMotionTime,
                                            /* eventTime= */ downMotionTime + 50,
                                            MotionEvent.ACTION_UP));

                    // Assert:
                    // (1) OnPeripheralClickListener shouldn't handle any MotionEvent that wasn't
                    //     from a peripheral;
                    // (2) OnPeripheralClickRunnable shouldn't be run.
                    assertFalse(downMotionHandled);
                    assertFalse(upMotionHandled);
                    assertEquals(0, onPeripheralClickRunnable.mNumTimesRun);
                });
    }

    @Test
    @SmallTest
    public void testClickFromMouse() {
        View testView = setUpTestView();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Setup
                    TestOnPeripheralClickRunnable onPeripheralClickRunnable =
                            new TestOnPeripheralClickRunnable();
                    testView.setOnTouchListener(
                            new OnPeripheralClickListener(testView, onPeripheralClickRunnable));

                    // Act
                    long downMotionTime = SystemClock.uptimeMillis();
                    boolean downMotionHandled =
                            testView.dispatchTouchEvent(
                                    MotionEventTestUtils.createMouseMotionEvent(
                                            downMotionTime,
                                            /* eventTime= */ downMotionTime,
                                            MotionEvent.ACTION_DOWN));
                    boolean upMotionHandled =
                            testView.dispatchTouchEvent(
                                    MotionEventTestUtils.createMouseMotionEvent(
                                            downMotionTime,
                                            /* eventTime= */ downMotionTime + 50,
                                            MotionEvent.ACTION_UP));

                    // Assert:
                    // (1) OnPeripheralClickListener should handle MotionEvents that were
                    //     from a peripheral;
                    // (2) OnPeripheralClickRunnable should be run.
                    assertTrue(downMotionHandled);
                    assertTrue(upMotionHandled);
                    assertEquals(1, onPeripheralClickRunnable.mNumTimesRun);
                });
    }

    @SuppressLint("SetTextI18n")
    private View setUpTestView() {
        Activity testActivity = mActivityRule.launchActivity(/* startIntent= */ null);
        TextView textView = new TextView(testActivity);
        textView.setText("Test View to be clicked");

        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        testActivity.setContentView(
                                textView,
                                new LayoutParams(
                                        LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT)));

        return textView;
    }

    private static final class TestOnPeripheralClickRunnable implements OnPeripheralClickRunnable {

        int mNumTimesRun;

        @Override
        public void run(MotionEventInfo triggeringMotion) {
            mNumTimesRun++;
        }
    }
}
