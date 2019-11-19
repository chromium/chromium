// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player;

import android.graphics.Rect;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * Tests for the {@link PaintPreviewFrame} class.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class PlayerManagerTest {
    /**
     * Tests the {@link PlayerManager#buildFrameTreeHierarchy} method with a simple frame
     * that has no sub-frames.
     */
    @Test
    public void testFrameHierarchyBuilderNoSubFrames() {
        PaintPreviewFrame generatedFrame1 = PlayerManager.buildFrameTreeHierarchy(34213523421L,
                new long[] {34213523421L}, new int[] {500, 600}, new int[] {0}, null, null);
        PaintPreviewFrame expectedFrame1 = PaintPreviewFrame.createInstanceForTest(
                34213523421L, 500, 600, new PaintPreviewFrame[] {}, new Rect[] {});
        Assert.assertEquals(expectedFrame1, generatedFrame1);
    }

    /**
     * Tests the {@link PlayerManager#buildFrameTreeHierarchy} method with a main frame
     * that has one nested sub-frame.
     */
    @Test
    public void testFrameHierarchyBuilderOneSubFrame() {
        PaintPreviewFrame generatedFrame2 = PlayerManager.buildFrameTreeHierarchy(
                23874287482142734L, new long[] {23874287482142734L, 34747571234L},
                new int[] {500, 600, 100, 200}, new int[] {1, 0}, new long[] {34747571234L},
                new int[] {10, 10, 50, 50});
        PaintPreviewFrame expectedFrame2Subframe = PaintPreviewFrame.createInstanceForTest(
                34747571234L, 100, 200, new PaintPreviewFrame[] {}, new Rect[] {});
        PaintPreviewFrame expectedFrame2 = PaintPreviewFrame.createInstanceForTest(
                23874287482142734L, 500, 600, new PaintPreviewFrame[] {expectedFrame2Subframe},
                new Rect[] {new Rect(10, 10, 60, 60)});
        Assert.assertEquals(expectedFrame2, generatedFrame2);
    }

    /**
     * Tests the {@link PlayerManager#buildFrameTreeHierarchy} method with a main frame
     * that has two levels of nested sub-frames. The first level is the main frame. In the second
     * frame, there is one sub-frame. In the third level, the same sub-frame is repeated twice in
     * different positions.
     */
    @Test
    public void testFrameHierarchyBuilderTwoSubFrames() {
        PaintPreviewFrame generatedFrame3 =
                PlayerManager.buildFrameTreeHierarchy(8475737372237427342L,
                        new long[] {8475737372237427342L, 218932173213L, 39728389472348L},
                        new int[] {500, 600, 200, 100, 10, 20}, new int[] {1, 2, 0},
                        new long[] {218932173213L, 39728389472348L, 39728389472348L},
                        new int[] {50, 60, 100, 150, 10, 15, 20, 25, 30, 35, 5, 15});
        PaintPreviewFrame expectedFrame3Subframe2 = PaintPreviewFrame.createInstanceForTest(
                39728389472348L, 10, 20, new PaintPreviewFrame[] {}, new Rect[] {});
        PaintPreviewFrame expectedFrame3Subframe1 =
                PaintPreviewFrame.createInstanceForTest(218932173213L, 200, 100,
                        new PaintPreviewFrame[] {expectedFrame3Subframe2, expectedFrame3Subframe2},
                        new Rect[] {new Rect(10, 15, 30, 40), new Rect(30, 35, 35, 50)});
        PaintPreviewFrame expectedFrame3 = PaintPreviewFrame.createInstanceForTest(
                8475737372237427342L, 500, 600, new PaintPreviewFrame[] {expectedFrame3Subframe1},
                new Rect[] {new Rect(50, 60, 150, 210)});
        Assert.assertEquals(expectedFrame3, generatedFrame3);
    }
}