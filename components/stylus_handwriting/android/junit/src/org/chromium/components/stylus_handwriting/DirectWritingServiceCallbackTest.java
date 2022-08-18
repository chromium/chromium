// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.stylus_handwriting;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.graphics.Point;
import android.graphics.PointF;
import android.graphics.Rect;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;

/**
 * Unit tests for {@link DirectWritingServiceCallback}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DirectWritingServiceCallbackTest {
    private static final String SAMPLE_INPUT = "sample input";

    private DirectWritingServiceCallback mDwServiceCallback = new DirectWritingServiceCallback();

    @Test
    @Feature({"Stylus Handwriting"})
    public void testInputStateData() {
        // Input state values are default when updateInputState is not called yet.
        assertEquals("", mDwServiceCallback.getText());
        assertEquals(0, mDwServiceCallback.getSelectionStart());
        assertEquals(0, mDwServiceCallback.getSelectionEnd());

        // Set input state params and verify.
        int selectionStart = 2;
        int selectionEnd = 2;
        mDwServiceCallback.updateInputState(SAMPLE_INPUT, selectionStart, selectionEnd);
        assertEquals(SAMPLE_INPUT, mDwServiceCallback.getText());
        assertEquals(selectionStart, mDwServiceCallback.getSelectionStart());
        assertEquals(selectionEnd, mDwServiceCallback.getSelectionEnd());

        // Verify edit bounds rect and cursor location point.
        assertTrue(mDwServiceCallback.getCursorLocation(0).equals(0, 0));
        assertEquals(0, mDwServiceCallback.getLeft());
        assertEquals(0, mDwServiceCallback.getRight());
        assertEquals(0, mDwServiceCallback.getTop());
        assertEquals(0, mDwServiceCallback.getBottom());
        Rect editBounds = new Rect(10, 20, 100, 200);
        Point cursorLocation = new Point(10, 20);
        mDwServiceCallback.updateEditableBounds(editBounds, cursorLocation);
        assertEquals(new PointF(cursorLocation), mDwServiceCallback.getCursorLocation(0));
        assertEquals(editBounds.left, mDwServiceCallback.getLeft());
        assertEquals(editBounds.right, mDwServiceCallback.getRight());
        assertEquals(editBounds.top, mDwServiceCallback.getTop());
        assertEquals(editBounds.bottom, mDwServiceCallback.getBottom());
    }
}