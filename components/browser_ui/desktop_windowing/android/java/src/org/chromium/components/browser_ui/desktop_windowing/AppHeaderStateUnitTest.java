// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.desktop_windowing;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import android.graphics.Rect;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit test for {@link AppHeaderState}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AppHeaderStateUnitTest {
    @Test
    public void testLeftRightPadding() {
        // Top padding: 0
        // Left padding: 10
        // Right padding: 20
        // Height: 50
        var appHeader = new AppHeaderState(new Rect(0, 0, 100, 50), new Rect(10, 0, 80, 50), true);
        assertTrue("AppHeaderState is valid.", appHeader.isValid());
        assertEquals("Left padding is wrong.", 10, appHeader.getLeftPadding());
        assertEquals("Right padding is wrong.", 20, appHeader.getRightPadding());
        assertEquals("Height is wrong.", 50, appHeader.getAppHeaderHeight());
        assertTrue("AppHeaderState should be in DW mode.", appHeader.isInDesktopWindow());
    }

    @Test
    public void testWithTopPadding() {
        // Top padding: 2
        // Left padding: 10
        // Right padding: 20
        // Height: 50
        var appHeader = new AppHeaderState(new Rect(0, 0, 100, 50), new Rect(10, 2, 80, 50), true);
        assertTrue("AppHeaderState is valid.", appHeader.isValid());
        assertEquals("Left padding is wrong.", 10, appHeader.getLeftPadding());
        assertEquals("Right padding is wrong.", 20, appHeader.getRightPadding());
        assertEquals("Height is wrong.", 48, appHeader.getAppHeaderHeight());
        assertTrue("AppHeaderState should be in DW mode.", appHeader.isInDesktopWindow());
    }

    @Test
    public void testEmpty() {
        var appHeader = new AppHeaderState();
        assertTrue("AppHeaderState is valid.", appHeader.isValid());
        assertEquals("Left padding is wrong.", 0, appHeader.getLeftPadding());
        assertEquals("Right padding is wrong.", 0, appHeader.getRightPadding());
        assertEquals("Height is wrong.", 0, appHeader.getAppHeaderHeight());
        assertFalse(
                "Empty AppHeaderState is not in desktop windowing mode.",
                appHeader.isInDesktopWindow());

        appHeader = new AppHeaderState(new Rect(0, 0, 100, 50), new Rect(), true);
        assertTrue("AppHeaderState is valid.", appHeader.isValid());
        assertEquals("Left padding is wrong.", 0, appHeader.getLeftPadding());
        assertEquals("Right padding is wrong.", 0, appHeader.getRightPadding());
        assertEquals("Height is wrong.", 0, appHeader.getAppHeaderHeight());
        assertTrue("AppHeaderState should be in DW mode.", appHeader.isInDesktopWindow());
    }

    @Test
    public void testInvalid() {
        assertFalse(
                "appWindow is empty.",
                new AppHeaderState(new Rect(), new Rect(1, 0, 10, 5), true).isValid());
        assertFalse(
                "widestUnoccludedRect not contained in appWindow.",
                new AppHeaderState(new Rect(0, 0, 10, 5), new Rect(0, 4, 10, 10), true).isValid());
        assertFalse(
                "widestUnoccludedRect not contained in appWindow.",
                new AppHeaderState(new Rect(0, 0, 10, 5), new Rect(0, 0, 11, 5), true).isValid());
    }

    @Test
    public void testIsEqual() {
        assertEquals(
                "States are the same.",
                new AppHeaderState(new Rect(0, 0, 10, 10), new Rect(0, 0, 1, 1), true),
                new AppHeaderState(new Rect(0, 0, 10, 10), new Rect(0, 0, 1, 1), true));
        assertEquals(
                "States are the same.",
                new AppHeaderState(new Rect(0, 0, 10, 10), new Rect(), true),
                new AppHeaderState(new Rect(0, 0, 10, 10), new Rect(), true));
        assertEquals(
                "States are the same even when they are invalid.",
                new AppHeaderState(new Rect(0, 0, 10, 10), new Rect(0, 0, 1, 1), false),
                new AppHeaderState(new Rect(0, 0, 10, 10), new Rect(0, 0, 1, 1), false));

        assertNotEquals(
                "isInDesktopWindow make the 2 state different.",
                new AppHeaderState(new Rect(), new Rect(), true),
                new AppHeaderState(new Rect(), new Rect(), false));

        assertNotEquals(
                "isInDesktopWindow make the 2 state different.",
                new AppHeaderState(new Rect(0, 0, 10, 10), new Rect(), true),
                new AppHeaderState(new Rect(0, 0, 10, 10), new Rect(), false));
        assertNotEquals(
                "widestUnoccludedRects are different.",
                new AppHeaderState(new Rect(0, 0, 10, 10), new Rect(0, 0, 1, 1), true),
                new AppHeaderState(new Rect(0, 0, 10, 10), new Rect(0, 0, 1, 2), true));
    }
}
