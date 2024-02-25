// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player;

import android.graphics.Rect;
import android.os.Parcel;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.UnguessableToken;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests for the {@link PaintPreviewFrame} class. */
@RunWith(BaseRobolectricTestRunner.class)
public class PlayerManagerTest {
    private UnguessableToken makeToken(long high, long low) {
        // Use a parcel for testing to avoid calling the normal native constructor.
        Parcel parcel = Parcel.obtain();
        parcel.writeLong(high);
        parcel.writeLong(low);
        parcel.setDataPosition(0);
        return UnguessableToken.CREATOR.createFromParcel(parcel);
    }

    /**
     * Tests the {@link PlayerManager#buildFrameTreeHierarchy} method with a simple frame that has
     * no sub-frames.
     */
    @Test
    public void testFrameHierarchyBuilderNoSubFrames() {
        UnguessableToken token = makeToken(3728193L, 3283974L);
        PaintPreviewFrame generatedFrame1 =
                PlayerManager.buildFrameTreeHierarchy(
                        token,
                        new UnguessableToken[] {token},
                        new int[] {500, 600},
                        new int[] {100, 200},
                        new int[] {0},
                        null,
                        null,
                        false);
        PaintPreviewFrame expectedFrame1 =
                PaintPreviewFrame.createInstanceForTest(
                        token, 500, 600, 100, 200, new PaintPreviewFrame[] {}, new Rect[] {});
        Assert.assertEquals(expectedFrame1, generatedFrame1);
    }

    /**
     * Tests the {@link PlayerManager#buildFrameTreeHierarchy} method with a main frame that has one
     * nested sub-frame.
     */
    @Test
    public void testFrameHierarchyBuilderOneSubFrame() {
        UnguessableToken mainToken = makeToken(1293L, 89798L);
        UnguessableToken subframeToken = makeToken(123982L, 637846L);
        PaintPreviewFrame generatedFrame2 =
                PlayerManager.buildFrameTreeHierarchy(
                        mainToken,
                        new UnguessableToken[] {mainToken, subframeToken},
                        new int[] {500, 600, 100, 200},
                        new int[] {50, 60, 0, 5},
                        new int[] {1, 0},
                        new UnguessableToken[] {subframeToken},
                        new int[] {10, 10, 50, 50},
                        false);
        PaintPreviewFrame expectedFrame2Subframe =
                PaintPreviewFrame.createInstanceForTest(
                        subframeToken, 100, 200, 0, 5, new PaintPreviewFrame[] {}, new Rect[] {});
        PaintPreviewFrame expectedFrame2 =
                PaintPreviewFrame.createInstanceForTest(
                        mainToken,
                        500,
                        600,
                        50,
                        60,
                        new PaintPreviewFrame[] {expectedFrame2Subframe},
                        new Rect[] {new Rect(10, 10, 60, 60)});
        Assert.assertEquals(expectedFrame2, generatedFrame2);
    }

    /**
     * Tests the {@link PlayerManager#buildFrameTreeHierarchy} method with a main frame that has two
     * levels of nested sub-frames. The first level is the main frame. In the second frame, there is
     * one sub-frame. In the third level, the same sub-frame is repeated twice in different
     * positions.
     */
    @Test
    public void testFrameHierarchyBuilderTwoSubFrames() {
        UnguessableToken mainToken = makeToken(9876L, 1234L);
        UnguessableToken subframe1Token = makeToken(32879342L, 2931920L);
        UnguessableToken subframe2Token = makeToken(989272L, 3789489L);
        PaintPreviewFrame generatedFrame3 =
                PlayerManager.buildFrameTreeHierarchy(
                        mainToken,
                        new UnguessableToken[] {mainToken, subframe1Token, subframe2Token},
                        new int[] {500, 600, 200, 100, 10, 20},
                        new int[] {50, 60, 20, 10, 1, 2},
                        new int[] {1, 2, 0},
                        new UnguessableToken[] {subframe1Token, subframe2Token, subframe2Token},
                        new int[] {50, 60, 100, 150, 10, 15, 20, 25, 30, 35, 5, 15},
                        false);
        PaintPreviewFrame expectedFrame3Subframe2 =
                PaintPreviewFrame.createInstanceForTest(
                        subframe2Token, 10, 20, 1, 2, new PaintPreviewFrame[] {}, new Rect[] {});
        PaintPreviewFrame expectedFrame3Subframe1 =
                PaintPreviewFrame.createInstanceForTest(
                        subframe1Token,
                        200,
                        100,
                        20,
                        10,
                        new PaintPreviewFrame[] {expectedFrame3Subframe2, expectedFrame3Subframe2},
                        new Rect[] {new Rect(10, 15, 30, 40), new Rect(30, 35, 35, 50)});
        PaintPreviewFrame expectedFrame3 =
                PaintPreviewFrame.createInstanceForTest(
                        mainToken,
                        500,
                        600,
                        50,
                        60,
                        new PaintPreviewFrame[] {expectedFrame3Subframe1},
                        new Rect[] {new Rect(50, 60, 150, 210)});
        Assert.assertEquals(expectedFrame3, generatedFrame3);
    }
}
