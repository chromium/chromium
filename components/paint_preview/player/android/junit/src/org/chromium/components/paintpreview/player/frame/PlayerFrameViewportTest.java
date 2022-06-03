// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player.frame;

import android.graphics.Rect;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * Tests for the {@link PlayerFrameViewport} class.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class PlayerFrameViewportTest {
    private static final float TOLERANCE = 0.001f;

    /**
     * Tests that viewport size is set/get correctly.
     */
    @Test
    public void testViewportSize() {
        PlayerFrameViewport viewport = new PlayerFrameViewport();
        viewport.setSize(100, 200);

        Assert.assertEquals(100, viewport.getWidth());
        Assert.assertEquals(200, viewport.getHeight());

        Rect viewportRect = viewport.asRect();
        Assert.assertEquals(0, viewportRect.left);
        Assert.assertEquals(0, viewportRect.top);
        Assert.assertEquals(100, viewportRect.width());
        Assert.assertEquals(200, viewportRect.height());
    }

    /**
     * Tests that viewport translation is set/get correctly.
     */
    @Test
    public void testViewportTranslation() {
        PlayerFrameViewport viewport = new PlayerFrameViewport();
        viewport.setSize(100, 200);

        viewport.offset(50, 60);
        Assert.assertEquals(50f, viewport.getTransX(), TOLERANCE);
        Assert.assertEquals(60f, viewport.getTransY(), TOLERANCE);

        Rect viewportRect = viewport.asRect();
        Assert.assertEquals(50, viewportRect.left);
        Assert.assertEquals(60, viewportRect.top);
        Assert.assertEquals(100, viewportRect.width());
        Assert.assertEquals(200, viewportRect.height());

        viewport.offset(-200, -110);
        Assert.assertEquals(-150f, viewport.getTransX(), TOLERANCE);
        Assert.assertEquals(-50f, viewport.getTransY(), TOLERANCE);

        viewportRect = viewport.asRect();
        Assert.assertEquals(-150, viewportRect.left);
        Assert.assertEquals(-50, viewportRect.top);
        Assert.assertEquals(100, viewportRect.width());
        Assert.assertEquals(200, viewportRect.height());

        viewport.setTrans(123, 456);
        Assert.assertEquals(123f, viewport.getTransX(), TOLERANCE);
        Assert.assertEquals(456f, viewport.getTransY(), TOLERANCE);

        viewportRect = viewport.asRect();
        Assert.assertEquals(123, viewportRect.left);
        Assert.assertEquals(456, viewportRect.top);
        Assert.assertEquals(100, viewportRect.width());
        Assert.assertEquals(200, viewportRect.height());
    }

    /**
     * Tests that viewport scaling works correctly.
     */
    @Test
    public void testViewportScaling() {
        PlayerFrameViewport viewport = new PlayerFrameViewport();
        viewport.setSize(100, 200);

        Assert.assertEquals(1f, viewport.getScale(), TOLERANCE);

        // Resetting the scale to 0 is an "invalid state" used to indicate the scale needs to be
        // updated.
        viewport.setScale(0f);
        Assert.assertEquals(0f, viewport.getScale(), TOLERANCE);

        viewport.setScale(2f);
        Assert.assertEquals(2f, viewport.getScale(), TOLERANCE);

        viewport.scale(0.5f, 0, 0);
        Assert.assertEquals(1f, viewport.getScale(), TOLERANCE);

        viewport.scale(0.5f, 0, 0);
        Assert.assertEquals(0.5f, viewport.getScale(), TOLERANCE);

        viewport.scale(5f, 0, 0);
        Assert.assertEquals(2.5f, viewport.getScale(), TOLERANCE);

        // Try scaling about a pivot.
        viewport.setTrans(100, 50);
        viewport.scale(0.5f, -30, -40);
        Assert.assertEquals(1.25f, viewport.getScale(), TOLERANCE);
        Assert.assertEquals(65f, viewport.getTransX(), TOLERANCE);
        Assert.assertEquals(45f, viewport.getTransY(), TOLERANCE);
    }
}
