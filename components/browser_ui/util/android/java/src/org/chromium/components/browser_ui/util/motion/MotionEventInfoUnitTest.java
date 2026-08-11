// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.util.motion;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.when;

import android.view.KeyEvent;
import android.view.MotionEvent;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link MotionEventInfo}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MotionEventInfoUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private MotionEvent mMotionEvent;

    @Test
    public void testFromMotionEvent() {
        when(mMotionEvent.getPointerCount()).thenReturn(2);
        when(mMotionEvent.getToolType(eq(0))).thenReturn(MotionEvent.TOOL_TYPE_FINGER);
        when(mMotionEvent.getToolType(eq(1))).thenReturn(MotionEvent.TOOL_TYPE_STYLUS);
        when(mMotionEvent.getAction()).thenReturn(MotionEvent.ACTION_DOWN);
        when(mMotionEvent.getSource()).thenReturn(1234);
        when(mMotionEvent.getMetaState())
                .thenReturn(KeyEvent.META_CTRL_ON | KeyEvent.META_SHIFT_ON);

        MotionEventInfo info = MotionEventInfo.fromMotionEvent(mMotionEvent);

        assertEquals(MotionEvent.ACTION_DOWN, info.action);
        assertEquals(1234, info.source);
        assertArrayEquals(
                new int[] {MotionEvent.TOOL_TYPE_FINGER, MotionEvent.TOOL_TYPE_STYLUS},
                info.toolType);
        assertEquals(KeyEvent.META_CTRL_ON | KeyEvent.META_SHIFT_ON, info.metaState);
    }

    @Test
    public void testModifiers_None() {
        when(mMotionEvent.getPointerCount()).thenReturn(0);
        when(mMotionEvent.getMetaState()).thenReturn(0);

        MotionEventInfo info = MotionEventInfo.fromMotionEvent(mMotionEvent);
        assertFalse(info.hasCtrlOrMeta());
        assertFalse(info.hasShift());
    }

    @Test
    public void testModifiers_Ctrl() {
        when(mMotionEvent.getPointerCount()).thenReturn(0);
        when(mMotionEvent.getMetaState()).thenReturn(KeyEvent.META_CTRL_ON);

        MotionEventInfo info = MotionEventInfo.fromMotionEvent(mMotionEvent);
        assertTrue(info.hasCtrlOrMeta());
        assertFalse(info.hasShift());
    }

    @Test
    public void testModifiers_Meta() {
        when(mMotionEvent.getPointerCount()).thenReturn(0);
        when(mMotionEvent.getMetaState()).thenReturn(KeyEvent.META_META_ON);

        MotionEventInfo info = MotionEventInfo.fromMotionEvent(mMotionEvent);
        assertTrue(info.hasCtrlOrMeta());
        assertFalse(info.hasShift());
    }

    @Test
    public void testModifiers_Shift() {
        when(mMotionEvent.getPointerCount()).thenReturn(0);
        when(mMotionEvent.getMetaState()).thenReturn(KeyEvent.META_SHIFT_ON);

        MotionEventInfo info = MotionEventInfo.fromMotionEvent(mMotionEvent);
        assertFalse(info.hasCtrlOrMeta());
        assertTrue(info.hasShift());
    }

    @Test
    public void testModifiers_CtrlAndShift() {
        when(mMotionEvent.getPointerCount()).thenReturn(0);
        when(mMotionEvent.getMetaState())
                .thenReturn(KeyEvent.META_CTRL_ON | KeyEvent.META_SHIFT_ON);

        MotionEventInfo info = MotionEventInfo.fromMotionEvent(mMotionEvent);
        assertTrue(info.hasCtrlOrMeta());
        assertTrue(info.hasShift());
    }
}
