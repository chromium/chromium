// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.graphics.Outline;
import android.graphics.Rect;
import android.view.View;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests for {@link RoundedCornerOutlineProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class RoundedCornerOutlineProviderUnitTest {
    private static final int VIEW_WIDTH = 120;
    private static final int VIEW_HEIGHT = 50;
    private static final int RADIUS = 10;

    View mView;

    private Outline mOutline;
    private Context mContext;
    private RoundedCornerOutlineProvider mProvider;
    private Rect mRect;

    @Before
    public void setUp() {
        mContext = RuntimeEnvironment.application;

        // Permit RTL tests.
        // Note that without this, all LayoutDirection changes will be ignored Views
        // and Robolectric - by default - does not support RTL mode (needs to be
        // explicitly enabled).
        mContext.getApplicationInfo().flags |= ApplicationInfo.FLAG_SUPPORTS_RTL;

        mView = new View(mContext);
        mView.layout(0, 0, VIEW_WIDTH, VIEW_HEIGHT);
        mProvider = new RoundedCornerOutlineProvider(RADIUS);
        mRect = new Rect();
        mOutline = new Outline();
    }

    @Test
    @SmallTest
    public void verifyCanClipWithRounding() {
        mProvider.getOutline(mView, mOutline);
        mOutline.getRect(mRect);
        Assert.assertEquals(new Rect(0, 0, VIEW_WIDTH, VIEW_HEIGHT), mRect);
        Assert.assertEquals(RADIUS, mOutline.getRadius(), 0.001);
        Assert.assertTrue(mOutline.canClip());
    }

    @Test
    @SmallTest
    public void verifyCanClipWithNoRounding() {
        mProvider = new RoundedCornerOutlineProvider();
        mProvider.getOutline(mView, mOutline);
        mOutline.getRect(mRect);
        Assert.assertEquals(new Rect(0, 0, VIEW_WIDTH, VIEW_HEIGHT), mRect);
        Assert.assertEquals(0, mOutline.getRadius(), 0.001);
        Assert.assertTrue(mOutline.canClip());
    }

    @Test
    @SmallTest
    public void verifyRespectsRoundingUpdates() {
        mProvider.setRadius(RADIUS * 3);
        mProvider.getOutline(mView, mOutline);
        Assert.assertEquals(RADIUS * 3, mOutline.getRadius(), 0.001);
        mProvider.setRadius(RADIUS * 2);
        mProvider.getOutline(mView, mOutline);
        Assert.assertEquals(RADIUS * 2, mOutline.getRadius(), 0.001);
        mProvider.setRadius(RADIUS * 5);
        mProvider.getOutline(mView, mOutline);
        Assert.assertEquals(RADIUS * 5, mOutline.getRadius(), 0.001);
        Assert.assertTrue(mOutline.canClip());
    }

    @Test
    @SmallTest
    public void verifyRespectsPaddings() {
        mView.setPaddingRelative(15, 10, 25, 20);
        mProvider.getOutline(mView, mOutline);

        // Default: no clipping padded area.
        mProvider.getOutline(mView, mOutline);
        mOutline.getRect(mRect);
        Assert.assertEquals(new Rect(0, 0, VIEW_WIDTH, VIEW_HEIGHT), mRect);
        Assert.assertEquals(RADIUS, mOutline.getRadius(), 0.001);
        Assert.assertTrue(mOutline.canClip());

        // Variant: clip padded area.
        mProvider.setClipPaddedArea(true);
        mProvider.getOutline(mView, mOutline);
        mOutline.getRect(mRect);
        Assert.assertEquals(new Rect(15, 10, VIEW_WIDTH - 25, VIEW_HEIGHT - 20), mRect);
        Assert.assertEquals(RADIUS, mOutline.getRadius(), 0.001);
        Assert.assertTrue(mOutline.canClip());
    }

    @Test
    @SmallTest
    public void verifyRespectsPaddingsInRTLMode() {
        mView.setLayoutDirection(View.LAYOUT_DIRECTION_RTL);
        Assert.assertEquals(
                "layout direction not supported",
                View.LAYOUT_DIRECTION_RTL,
                mView.getLayoutDirection());

        mView.setPaddingRelative(15, 10, 25, 20);

        // Default: no clipping padded area.
        mProvider.getOutline(mView, mOutline);
        mOutline.getRect(mRect);
        Assert.assertEquals(new Rect(0, 0, VIEW_WIDTH, VIEW_HEIGHT), mRect);
        Assert.assertEquals(RADIUS, mOutline.getRadius(), 0.001);
        Assert.assertTrue(mOutline.canClip());

        // Variant: clip padded area.
        mProvider.setClipPaddedArea(true);
        mProvider.getOutline(mView, mOutline);
        mOutline.getRect(mRect);
        Assert.assertEquals(new Rect(25, 10, VIEW_WIDTH - 15, VIEW_HEIGHT - 20), mRect);
        Assert.assertEquals(RADIUS, mOutline.getRadius(), 0.001);
        Assert.assertTrue(mOutline.canClip());
    }

    @Test
    @SmallTest
    public void verifyViewOriginDoesNotImpactOutline() {
        mView.layout(10, 15, 10 + VIEW_WIDTH, 15 + VIEW_HEIGHT);
        mView.setPaddingRelative(15, 10, 25, 20);

        // Default: no clipping padded area.
        mProvider.getOutline(mView, mOutline);
        mOutline.getRect(mRect);
        Assert.assertEquals(new Rect(0, 0, VIEW_WIDTH, VIEW_HEIGHT), mRect);
        Assert.assertEquals(RADIUS, mOutline.getRadius(), 0.001);
        Assert.assertTrue(mOutline.canClip());

        // Variant: clip padded area.
        mProvider.setClipPaddedArea(true);
        mProvider.getOutline(mView, mOutline);
        mOutline.getRect(mRect);
        Assert.assertEquals(new Rect(15, 10, VIEW_WIDTH - 25, VIEW_HEIGHT - 20), mRect);
        Assert.assertEquals(RADIUS, mOutline.getRadius(), 0.001);
        Assert.assertTrue(mOutline.canClip());
    }

    @Test
    @SmallTest
    public void verifyLeftEdgeExclusion() {
        mView.layout(10, 15, 10 + VIEW_WIDTH, 15 + VIEW_HEIGHT);
        mView.setPaddingRelative(15, 10, 25, 20);
        mProvider.setRoundingEdges(
                /* left= */ false, /* top= */ true, /* right= */ true, /* bottom= */ true);

        // Default: no clipping padded area.
        mProvider.getOutline(mView, mOutline);
        mOutline.getRect(mRect);
        Assert.assertEquals(new Rect(-RADIUS, 0, VIEW_WIDTH, VIEW_HEIGHT), mRect);
        Assert.assertEquals(RADIUS, mOutline.getRadius(), 0.001);
        Assert.assertTrue(mOutline.canClip());

        // Variant: clip padded area.
        mProvider.setClipPaddedArea(true);
        mProvider.getOutline(mView, mOutline);
        mOutline.getRect(mRect);
        Assert.assertEquals(new Rect(15 - RADIUS, 10, VIEW_WIDTH - 25, VIEW_HEIGHT - 20), mRect);
        Assert.assertEquals(RADIUS, mOutline.getRadius(), 0.001);
        Assert.assertTrue(mOutline.canClip());
    }

    @Test
    @SmallTest
    public void verifyTopEdgeExclusion() {
        mView.layout(10, 15, 10 + VIEW_WIDTH, 15 + VIEW_HEIGHT);
        mView.setPaddingRelative(15, 10, 25, 20);
        mProvider.setRoundingEdges(
                /* left= */ true, /* top= */ false, /* right= */ true, /* bottom= */ true);

        // Default: no clipping padded area.
        mProvider.getOutline(mView, mOutline);
        mOutline.getRect(mRect);
        Assert.assertEquals(new Rect(0, -RADIUS, VIEW_WIDTH, VIEW_HEIGHT), mRect);
        Assert.assertEquals(RADIUS, mOutline.getRadius(), 0.001);
        Assert.assertTrue(mOutline.canClip());

        // Variant: clip padded area.
        mProvider.setClipPaddedArea(true);
        mProvider.getOutline(mView, mOutline);
        mOutline.getRect(mRect);
        Assert.assertEquals(new Rect(15, 10 - RADIUS, VIEW_WIDTH - 25, VIEW_HEIGHT - 20), mRect);
        Assert.assertEquals(RADIUS, mOutline.getRadius(), 0.001);
        Assert.assertTrue(mOutline.canClip());
    }

    @Test
    @SmallTest
    public void verifyRightEdgeExclusion() {
        mView.layout(10, 15, 10 + VIEW_WIDTH, 15 + VIEW_HEIGHT);
        mView.setPaddingRelative(15, 10, 25, 20);
        mProvider.setRoundingEdges(
                /* left= */ true, /* top= */ true, /* right= */ false, /* bottom= */ true);

        // Default: no clipping padded area.
        mProvider.getOutline(mView, mOutline);
        mOutline.getRect(mRect);
        Assert.assertEquals(new Rect(0, 0, VIEW_WIDTH + RADIUS, VIEW_HEIGHT), mRect);
        Assert.assertEquals(RADIUS, mOutline.getRadius(), 0.001);
        Assert.assertTrue(mOutline.canClip());

        // Variant: clip padded area.
        mProvider.setClipPaddedArea(true);
        mProvider.getOutline(mView, mOutline);
        mOutline.getRect(mRect);
        Assert.assertEquals(new Rect(15, 10, VIEW_WIDTH - 25 + RADIUS, VIEW_HEIGHT - 20), mRect);
        Assert.assertEquals(RADIUS, mOutline.getRadius(), 0.001);
        Assert.assertTrue(mOutline.canClip());
    }

    @Test
    @SmallTest
    public void verifyBottomEdgeExclusion() {
        mView.layout(10, 15, 10 + VIEW_WIDTH, 15 + VIEW_HEIGHT);
        mView.setPaddingRelative(15, 10, 25, 20);
        mProvider.setRoundingEdges(
                /* left= */ true, /* top= */ true, /* right= */ true, /* bottom= */ false);

        // Default: no clipping padded area.
        mProvider.getOutline(mView, mOutline);
        mOutline.getRect(mRect);
        Assert.assertEquals(new Rect(0, 0, VIEW_WIDTH, VIEW_HEIGHT + RADIUS), mRect);
        Assert.assertEquals(RADIUS, mOutline.getRadius(), 0.001);
        Assert.assertTrue(mOutline.canClip());

        // Variant: clip padded area.
        mProvider.setClipPaddedArea(true);
        mProvider.getOutline(mView, mOutline);
        mOutline.getRect(mRect);
        Assert.assertEquals(new Rect(15, 10, VIEW_WIDTH - 25, VIEW_HEIGHT - 20 + RADIUS), mRect);
        Assert.assertEquals(RADIUS, mOutline.getRadius(), 0.001);
        Assert.assertTrue(mOutline.canClip());
    }

    @Test
    @SmallTest
    public void verifyRightBottomEdgeExclusion() {
        mView.layout(10, 15, 10 + VIEW_WIDTH, 15 + VIEW_HEIGHT);
        mView.setPaddingRelative(15, 10, 25, 20);
        // Disable rounding near the right and bottom edges.
        // The effect is that only top-left edge is rounded.
        mProvider.setRoundingEdges(
                /* left= */ true, /* top= */ true, /* right= */ false, /* bottom= */ false);

        // Default: no clipping padded area.
        mProvider.getOutline(mView, mOutline);
        mOutline.getRect(mRect);
        Assert.assertEquals(new Rect(0, 0, VIEW_WIDTH + RADIUS, VIEW_HEIGHT + RADIUS), mRect);
        Assert.assertEquals(RADIUS, mOutline.getRadius(), 0.001);
        Assert.assertTrue(mOutline.canClip());

        // Variant: clip padded area.
        mProvider.setClipPaddedArea(true);
        mProvider.getOutline(mView, mOutline);
        mOutline.getRect(mRect);
        Assert.assertEquals(
                new Rect(15, 10, VIEW_WIDTH - 25 + RADIUS, VIEW_HEIGHT - 20 + RADIUS), mRect);
        Assert.assertEquals(RADIUS, mOutline.getRadius(), 0.001);
        Assert.assertTrue(mOutline.canClip());
    }

    @Test
    @SmallTest
    public void verifyTopBottomEdgeExclusion() {
        mView.layout(10, 15, 10 + VIEW_WIDTH, 15 + VIEW_HEIGHT);
        mView.setPaddingRelative(15, 10, 25, 20);
        // Disable rounding near the top and bottom edges.
        // The effect is that rounding is effectively disabled.
        mProvider.setRoundingEdges(
                /* left= */ true, /* top= */ false, /* right= */ true, /* bottom= */ false);

        // Default: no clipping padded area.
        mProvider.getOutline(mView, mOutline);
        mOutline.getRect(mRect);
        Assert.assertEquals(new Rect(0, -RADIUS, VIEW_WIDTH, VIEW_HEIGHT + RADIUS), mRect);
        Assert.assertEquals(RADIUS, mOutline.getRadius(), 0.001);
        Assert.assertTrue(mOutline.canClip());

        // Variant: clip padded area.
        mProvider.setClipPaddedArea(true);
        mProvider.getOutline(mView, mOutline);
        mOutline.getRect(mRect);
        Assert.assertEquals(
                new Rect(15, 10 - RADIUS, VIEW_WIDTH - 25, VIEW_HEIGHT - 20 + RADIUS), mRect);
        Assert.assertEquals(RADIUS, mOutline.getRadius(), 0.001);
        Assert.assertTrue(mOutline.canClip());
    }
}
