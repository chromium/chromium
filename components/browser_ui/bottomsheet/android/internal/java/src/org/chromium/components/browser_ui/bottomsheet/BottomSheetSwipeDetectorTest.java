// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.bottomsheet;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.view.MotionEvent;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.MathUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetSwipeDetector.SwipeableBottomSheet;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for the {@link BottomSheetSwipeDetector} class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public final class BottomSheetSwipeDetectorTest {
    /** The minimum height of the bottom sheet. */
    private static final float MIN_SHEET_OFFSET = 100;

    /** An arbitrary screen height. */
    private static final float SCREEN_HEIGHT = 1000;

    /** An instance of the mock swipable sheet. */
    private MockSwipeableBottomSheet mSwipeableBottomSheet;

    /** The swipe detector to process motion events. */
    private BottomSheetSwipeDetector mSwipeDetector;

    /** A mock implementation of a swipeable bottom sheet. */
    private static class MockSwipeableBottomSheet implements SwipeableBottomSheet {
        /** The minimum offset of the sheet. */
        private final float mMinOffset;

        /** The maximum offset of the sheet. */
        private final float mMaxOffset;

        /** Whether the content in the sheet is currently scrolled to the top. */
        public boolean isContentScrolledToTop;

        /** Whether the sheet should currently be animating. */
        public boolean shouldBeAnimating;

        /** The current offset of the bottom sheet. */
        private float mCurrentSheetOffset;

        public MockSwipeableBottomSheet(float minOffset, float maxOffset) {
            mMinOffset = minOffset;
            mMaxOffset = maxOffset;

            // The sheet should be initialized at the minimum state.
            mCurrentSheetOffset = mMinOffset;

            isContentScrolledToTop = true;
        }

        @Override
        public boolean isContentScrolledToTop() {
            return isContentScrolledToTop;
        }

        @Override
        public float getCurrentOffsetPx() {
            return mCurrentSheetOffset;
        }

        @Override
        public float getMinOffsetPx() {
            return mMinOffset;
        }

        @Override
        public float getMaxOffsetPx() {
            return mMaxOffset;
        }

        @Override
        public boolean isTouchEventInToolbar(MotionEvent event) {
            // This will be implementation specific in practice. This checks that the motion event
            // occured above the bottom of the toolbar.
            return event.getRawY() < (mMaxOffset - mCurrentSheetOffset) + mMinOffset;
        }

        @Override
        public boolean shouldGestureMoveSheet(
                MotionEvent initialDownEvent, MotionEvent currentEvent) {
            return true;
        }

        @Override
        public void setSheetOffset(float offset, boolean shouldAnimate) {
            mCurrentSheetOffset = offset;
            shouldBeAnimating = shouldAnimate;
        }
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mSwipeableBottomSheet = new MockSwipeableBottomSheet(MIN_SHEET_OFFSET, SCREEN_HEIGHT);
        mSwipeDetector = new BottomSheetSwipeDetector(null, mSwipeableBottomSheet);
    }

    /**
     * Create a list of motion events simulating a scroll event stream from (x1, y1) to (x2, y2) and
     * apply it to the provided swipe detector.
     *
     * @param x1 The start x.
     * @param y1 The start y.
     * @param x2 The end x.
     * @param y2 The end y.
     * @param detector The detector to apply the swipe to.
     * @param endScroll Whether or not to include the up event at the end of the stream.
     */
    private static void performScroll(
            float x1,
            float y1,
            float x2,
            float y2,
            BottomSheetSwipeDetector detector,
            boolean endScroll) {
        int moveEventCount = 10;

        ArrayList<MotionEvent> eventStream = new ArrayList<>();
        float xInterval = (x2 - x1) / moveEventCount;
        float yInterval = (y2 - y1) / moveEventCount;
        eventStream.add(MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, x1, y1, 0));
        for (int i = 0; i < moveEventCount; i++) {
            eventStream.add(
                    MotionEvent.obtain(
                            0,
                            0,
                            MotionEvent.ACTION_MOVE,
                            x1 + ((i + 1) * xInterval),
                            y1 + ((i + 1) * yInterval),
                            0));
        }
        if (endScroll) eventStream.add(MotionEvent.obtain(0, 0, MotionEvent.ACTION_UP, x2, y2, 0));

        applyGestureStream(eventStream, detector);
    }

    /**
     * Apply a list of events to a swipe detector.
     *
     * @param stream The list of motion events to apply to the detector.
     * @param detector The detector to apply the swipe to.
     */
    private static void applyGestureStream(
            List<MotionEvent> stream, BottomSheetSwipeDetector detector) {
        for (MotionEvent e : stream) {
            if (!detector.isScrolling()) {
                detector.onInterceptTouchEvent(e);
            } else {
                detector.onTouchEvent(e);
            }
        }
    }

    /** Test that the sheet moves when scrolled up from min height. */
    @Test
    public void testScrollToolbarUp_minHeight() {
        assertEquals(
                "The sheet should be at the minimum state.",
                MIN_SHEET_OFFSET,
                mSwipeableBottomSheet.getCurrentOffsetPx(),
                MathUtils.EPSILON);
        final float halfScreenHeight = SCREEN_HEIGHT / 2f;

        // Scrolling up half the screen should put the sheet at half + the min offset.
        performScroll(0, SCREEN_HEIGHT, 0, halfScreenHeight, mSwipeDetector, true);

        assertEquals(
                "The sheet is not at the correct height.",
                halfScreenHeight + MIN_SHEET_OFFSET,
                mSwipeableBottomSheet.getCurrentOffsetPx(),
                MathUtils.EPSILON);
        assertTrue("The sheet should be set to animate.", mSwipeableBottomSheet.shouldBeAnimating);
    }

    /** Test that the sheet is not told to animate mid-stream. */
    @Test
    public void testScrollToolbarUp_minHeight_noUpEvent() {
        final float halfScreenHeight = SCREEN_HEIGHT / 2f;

        performScroll(0, SCREEN_HEIGHT, 0, halfScreenHeight, mSwipeDetector, false);

        assertEquals(
                "The sheet is not at the correct height.",
                halfScreenHeight + MIN_SHEET_OFFSET,
                mSwipeableBottomSheet.getCurrentOffsetPx(),
                MathUtils.EPSILON);
        assertFalse(
                "The sheet should not be set to animate.", mSwipeableBottomSheet.shouldBeAnimating);
    }

    /** Test that the sheet does not move when scrolled up from max height. */
    @Test
    public void testScrollToolbarUp_maxHeight() {
        // Init the sheet to be full height.
        mSwipeableBottomSheet.setSheetOffset(SCREEN_HEIGHT, false);

        assertEquals(
                "The sheet should be at the maximum state.",
                SCREEN_HEIGHT,
                mSwipeableBottomSheet.getCurrentOffsetPx(),
                MathUtils.EPSILON);

        performScroll(0, 0, 0, -500, mSwipeDetector, true);

        assertEquals(
                "The sheet should still be at the maximum state.",
                SCREEN_HEIGHT,
                mSwipeableBottomSheet.getCurrentOffsetPx(),
                MathUtils.EPSILON);
        assertFalse(
                "The sheet should not be set to animate.", mSwipeableBottomSheet.shouldBeAnimating);
    }

    /** Test that the sheet does not move when scrolled down from min height. */
    @Test
    public void testScrollToolbarDown_minHeight() {
        assertEquals(
                "The sheet should be at the minimum state.",
                MIN_SHEET_OFFSET,
                mSwipeableBottomSheet.getCurrentOffsetPx(),
                MathUtils.EPSILON);

        performScroll(0, SCREEN_HEIGHT, 0, SCREEN_HEIGHT + 500, mSwipeDetector, true);

        assertEquals(
                "The sheet should still be at the minimum state.",
                MIN_SHEET_OFFSET,
                mSwipeableBottomSheet.getCurrentOffsetPx(),
                MathUtils.EPSILON);
        assertFalse(
                "The sheet should not be set to animate.", mSwipeableBottomSheet.shouldBeAnimating);
    }

    /** Test that the sheet moves when scrolled down from max height. */
    @Test
    public void testScrollToolbarDown_maxHeight() {
        // Init the sheet to be full height.
        mSwipeableBottomSheet.setSheetOffset(SCREEN_HEIGHT, false);

        assertEquals(
                "The sheet should be at the maximum state.",
                SCREEN_HEIGHT,
                mSwipeableBottomSheet.getCurrentOffsetPx(),
                MathUtils.EPSILON);
        final float halfScreenHeight = SCREEN_HEIGHT / 2f;

        // Scrolling down half the screen should put the sheet at half height.
        performScroll(0, 0, 0, halfScreenHeight, mSwipeDetector, true);

        assertEquals(
                "The sheet is not at the correct height.",
                halfScreenHeight,
                mSwipeableBottomSheet.getCurrentOffsetPx(),
                MathUtils.EPSILON);
        assertTrue("The sheet should be set to animate.", mSwipeableBottomSheet.shouldBeAnimating);
    }

    /**
     * Test that the sheet moves when scrolled down from max height while the content has been
     * scrolled.
     */
    @Test
    public void testScrollToolbarDown_maxHeight_contentScrolled() {
        // Init the sheet to be full height.
        mSwipeableBottomSheet.setSheetOffset(SCREEN_HEIGHT, false);

        assertEquals(
                "The sheet should be at the maximum state.",
                SCREEN_HEIGHT,
                mSwipeableBottomSheet.getCurrentOffsetPx(),
                MathUtils.EPSILON);
        final float halfScreenHeight = SCREEN_HEIGHT / 2f;

        // Scrolling down half the screen should put the sheet at half height, regardless of the
        // state of the content.
        performScroll(0, 0, 0, halfScreenHeight, mSwipeDetector, true);

        assertEquals(
                "The sheet is not at the correct height.",
                halfScreenHeight,
                mSwipeableBottomSheet.getCurrentOffsetPx(),
                MathUtils.EPSILON);
        assertTrue("The sheet should be set to animate.", mSwipeableBottomSheet.shouldBeAnimating);
    }

    /** Test that the sheet does not move when a scroll is not sufficiently in the up direction. */
    @Test
    public void testScrollToolbarDiagonal_minHeight() {
        assertEquals(
                "The sheet should be at the minimum state.",
                MIN_SHEET_OFFSET,
                mSwipeableBottomSheet.getCurrentOffsetPx(),
                MathUtils.EPSILON);
        final float halfScreenHeight = SCREEN_HEIGHT / 2f;

        performScroll(
                0, halfScreenHeight, halfScreenHeight, halfScreenHeight, mSwipeDetector, true);

        assertEquals(
                "The sheet should still be at the minimum state.",
                MIN_SHEET_OFFSET,
                mSwipeableBottomSheet.getCurrentOffsetPx(),
                MathUtils.EPSILON);
        assertFalse(
                "The sheet should not be set to animate.", mSwipeableBottomSheet.shouldBeAnimating);
    }

    /**
     * Test that the sheet does not move when the content is scrolled up and the sheet is at max
     * height.
     */
    @Test
    public void testScrollContent_maxHeight() {
        // Init the sheet to be full height.
        mSwipeableBottomSheet.setSheetOffset(SCREEN_HEIGHT, false);

        // Content is scrolled some amount.
        mSwipeableBottomSheet.isContentScrolledToTop = false;

        final float halfScreenHeight = SCREEN_HEIGHT / 2f;

        // Scroll down half the screen. The sheet should not move since the content is scrolled.
        performScroll(0, halfScreenHeight, 0, SCREEN_HEIGHT, mSwipeDetector, true);

        assertEquals(
                "The sheet should still be at the maximum state.",
                SCREEN_HEIGHT,
                mSwipeableBottomSheet.getCurrentOffsetPx(),
                MathUtils.EPSILON);
    }

    /**
     * Test that the sheet moves when a scroll occurs on the body of the sheet. Content should only
     * scroll if the sheet is at max height.
     */
    @Test
    public void testScrollContent_halfHeight() {
        final float halfScreenHeight = SCREEN_HEIGHT / 2f;

        // Init the sheet to be half height.
        mSwipeableBottomSheet.setSheetOffset(halfScreenHeight, false);

        // Content is scrolled some amount.
        mSwipeableBottomSheet.isContentScrolledToTop = false;

        // Scroll down on the content, the sheet should move.
        performScroll(0, halfScreenHeight / 2f, 0, SCREEN_HEIGHT, mSwipeDetector, true);

        assertEquals(
                "The sheet should be at the minimum state.",
                MIN_SHEET_OFFSET,
                mSwipeableBottomSheet.getCurrentOffsetPx(),
                MathUtils.EPSILON);
    }
}
