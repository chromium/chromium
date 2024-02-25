// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player.frame;

import android.graphics.Point;
import android.graphics.Rect;
import android.util.Size;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests for the {@link PlayerFrameViewport} class. */
@RunWith(BaseRobolectricTestRunner.class)
public class PlayerFrameViewportTest {
    private static final float TOLERANCE = 0.001f;

    /** Tests that viewport size is set/get correctly. */
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

        Rect visibleRect = viewport.getVisibleViewport(false);
        Assert.assertEquals(0, visibleRect.left);
        Assert.assertEquals(0, visibleRect.top);
        Assert.assertEquals(100, visibleRect.width());
        Assert.assertEquals(200, visibleRect.height());
    }

    /** Tests that viewport size is set/get correctly. */
    @Test
    public void testVisibleViewport() {
        PlayerFrameViewport viewport = new PlayerFrameViewport();
        viewport.setSize(100, 200);

        Assert.assertEquals(100, viewport.getWidth());
        Assert.assertEquals(200, viewport.getHeight());

        Rect viewportRect = viewport.asRect();
        Assert.assertEquals(0, viewportRect.left);
        Assert.assertEquals(0, viewportRect.top);
        Assert.assertEquals(100, viewportRect.width());
        Assert.assertEquals(200, viewportRect.height());

        Rect visibleRect = viewport.getVisibleViewport(true);
        Assert.assertEquals(0, visibleRect.left);
        Assert.assertEquals(0, visibleRect.top);
        Assert.assertEquals(0, visibleRect.width());
        Assert.assertEquals(0, visibleRect.height());
        Point offset = viewport.getOffset();
        Assert.assertEquals(0, offset.x);
        Assert.assertEquals(0, offset.y);

        Assert.assertTrue(viewport.isVisible(false));
        Assert.assertFalse(viewport.isVisible(true));

        viewport.setVisibleRegion(10, 25, 30, 55);
        visibleRect = viewport.getVisibleViewport(true);
        Assert.assertEquals(10, visibleRect.left);
        Assert.assertEquals(25, visibleRect.top);
        Assert.assertEquals(20, visibleRect.width());
        Assert.assertEquals(30, visibleRect.height());
        offset = viewport.getOffset();
        Assert.assertEquals(10, offset.x);
        Assert.assertEquals(25, offset.y);
        Assert.assertTrue(viewport.isVisible(true));

        viewport.setTrans(5, 15);
        visibleRect = viewport.getVisibleViewport(true);
        Assert.assertEquals(15, visibleRect.left);
        Assert.assertEquals(40, visibleRect.top);
        Assert.assertEquals(20, visibleRect.width());
        Assert.assertEquals(30, visibleRect.height());
        offset = viewport.getOffset();
        Assert.assertEquals(10, offset.x);
        Assert.assertEquals(25, offset.y);
        Assert.assertTrue(viewport.isVisible(true));
    }

    /** Tests that bitmap tile size is set correctly. */
    @Test
    public void testBitmapTileSize() {
        PlayerFrameViewport viewport = new PlayerFrameViewport();
        viewport.setSize(100, 300);

        Assert.assertEquals(100, viewport.getWidth());
        Assert.assertEquals(300, viewport.getHeight());

        Size tileSize = viewport.getBitmapTileSize();
        Assert.assertEquals(100, tileSize.getWidth());
        Assert.assertEquals(150, tileSize.getHeight());

        viewport.setSize(50, 400);
        tileSize = viewport.getBitmapTileSize();
        Assert.assertEquals(50, tileSize.getWidth());
        Assert.assertEquals(200, tileSize.getHeight());

        viewport.overrideTileSize(10, 20);
        viewport.setSize(40, 100);
        tileSize = viewport.getBitmapTileSize();
        Assert.assertEquals(10, tileSize.getWidth());
        Assert.assertEquals(20, tileSize.getHeight());

        viewport.overrideTileSize(4000, 4000);
        tileSize = viewport.getBitmapTileSize();
        Assert.assertEquals(2500, tileSize.getWidth());
        Assert.assertEquals(2500, tileSize.getHeight());
    }

    /** Tests that viewport translation is set/get correctly. */
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

    /** Tests that viewport scaling works correctly. */
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
