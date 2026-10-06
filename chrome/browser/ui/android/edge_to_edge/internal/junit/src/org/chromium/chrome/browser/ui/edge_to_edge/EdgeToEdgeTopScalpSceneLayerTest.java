// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.graphics.Color;
import android.graphics.RectF;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.cc.input.OffsetTag;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayer;

@RunWith(BaseRobolectricTestRunner.class)
public class EdgeToEdgeTopScalpSceneLayerTest {
    @Rule public MockitoRule mMockitoJUnit = MockitoJUnit.rule();
    @Mock private Runnable mRequestRenderRunnable;
    @Mock private EdgeToEdgeTopScalpSceneLayerJni mSceneLayerJni;
    @Mock private SceneLayer mContentTree;
    private EdgeToEdgeTopScalpSceneLayer mSceneLayer;

    @Before
    public void setUp() {
        doReturn(123L).when(mSceneLayerJni).init(any());
        EdgeToEdgeTopScalpSceneLayerJni.setInstanceForTesting(mSceneLayerJni);
        mSceneLayer = new EdgeToEdgeTopScalpSceneLayer(mRequestRenderRunnable);
    }

    @After
    public void tearDown() {
        EdgeToEdgeTopScalpSceneLayerJni.setInstanceForTesting(null);
    }

    @Test
    public void testUpdatesRequestRender() {
        mSceneLayer.setIsVisible(true);
        verify(mRequestRenderRunnable).run();

        // Setting same visibility should not trigger runnable again.
        mSceneLayer.setIsVisible(true);
        verify(mRequestRenderRunnable, times(1)).run();

        mSceneLayer.setHeight(30);
        verify(mRequestRenderRunnable, times(2)).run();

        // Setting same height should not trigger runnable again.
        mSceneLayer.setHeight(30);
        verify(mRequestRenderRunnable, times(2)).run();

        mSceneLayer.setColor(Color.RED);
        verify(mRequestRenderRunnable, times(3)).run();

        // Setting same color should not trigger runnable again.
        mSceneLayer.setColor(Color.RED);
        verify(mRequestRenderRunnable, times(3)).run();
    }

    @Test
    public void testNullRequestRenderRunnable_NoCrash() {
        EdgeToEdgeTopScalpSceneLayer layer = new EdgeToEdgeTopScalpSceneLayer(null);
        layer.setIsVisible(true);
        layer.setHeight(30);
        layer.setColor(Color.RED);
        layer.setYOffset(10);
    }

    @Test
    public void testGetUpdatedSceneOverlayTree() {
        mSceneLayer.setYOffset(12);
        mSceneLayer.setIsVisible(true);
        mSceneLayer.setHeight(30);
        mSceneLayer.setColor(Color.RED);
        OffsetTag offsetTag = new OffsetTag(Token.EMPTY);
        mSceneLayer.setOffsetTag(offsetTag);

        RectF viewport = new RectF(0, 0, 100, 400);
        mSceneLayer.getUpdatedSceneOverlayTree(viewport, viewport, null);
        verify(mSceneLayerJni)
                .updateEdgeToEdgeTopScalpLayer(
                        123, (int) viewport.width(), 30, Color.RED, 12, offsetTag);
    }

    @Test
    public void testSetContentTree() {
        mSceneLayer.setContentTree(mContentTree);
        verify(mSceneLayerJni).setContentTree(123, mContentTree);
    }

    @Test
    public void testIsSceneOverlayTreeShowing() {
        mSceneLayer.setIsVisible(true);
        assertTrue(mSceneLayer.isSceneOverlayTreeShowing());

        mSceneLayer.setIsVisible(false);
        assertFalse(mSceneLayer.isSceneOverlayTreeShowing());
    }
}
