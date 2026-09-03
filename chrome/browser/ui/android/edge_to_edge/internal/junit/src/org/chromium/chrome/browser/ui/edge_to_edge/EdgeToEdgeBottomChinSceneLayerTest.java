// Copyright 2024 The Chromium Authors
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
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.cc.input.OffsetTag;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

@RunWith(BaseRobolectricTestRunner.class)
public class EdgeToEdgeBottomChinSceneLayerTest {
    @Rule public MockitoRule mMockitoJUnit = MockitoJUnit.rule();
    @Mock private EdgeToEdgeBottomChinSceneLayerJni mSceneLayerJni;
    @Mock private Runnable mRequestRenderRunnable;
    private EdgeToEdgeBottomChinSceneLayer mSceneLayer;

    @Before
    public void setUp() {
        doReturn(123L).when(mSceneLayerJni).init(any());
        EdgeToEdgeBottomChinSceneLayerJni.setInstanceForTesting(mSceneLayerJni);
        mSceneLayer = new EdgeToEdgeBottomChinSceneLayer(mRequestRenderRunnable);
    }

    @After
    public void tearDown() {
        EdgeToEdgeBottomChinSceneLayerJni.setInstanceForTesting(null);
    }

    @Test
    public void testGetUpdatedSceneOverlayTree() {
        mSceneLayer.setYOffset(12);
        mSceneLayer.setIsVisible(true);
        mSceneLayer.setHeight(30);
        mSceneLayer.setColor(Color.RED);
        mSceneLayer.setDividerColor(Color.BLACK);
        OffsetTag offsetTag = new OffsetTag(Token.EMPTY);
        mSceneLayer.setOffsetTag(offsetTag);

        RectF viewport = new RectF(0, 0, 100, 400);
        mSceneLayer.getUpdatedSceneOverlayTree(viewport, viewport, null);
        verify(mSceneLayerJni)
                .updateEdgeToEdgeBottomChinLayer(
                        123,
                        (int) viewport.width(),
                        30,
                        Color.RED,
                        Color.BLACK,
                        viewport.height() + 12,
                        false,
                        offsetTag);
    }

    @Test
    public void testRequestRenderRunnable_NonNull() {
        mSceneLayer.setIsVisible(true);
        verify(mRequestRenderRunnable).run();

        mSceneLayer.setHeight(30);
        verify(mRequestRenderRunnable, times(2)).run();

        mSceneLayer.setColor(Color.RED);
        verify(mRequestRenderRunnable, times(3)).run();

        mSceneLayer.setDividerColor(Color.BLACK);
        verify(mRequestRenderRunnable, times(4)).run();
    }

    @Test
    public void testRequestRenderRunnable_Null() {
        EdgeToEdgeBottomChinSceneLayer sceneLayer = new EdgeToEdgeBottomChinSceneLayer(null);
        sceneLayer.setIsVisible(true);
        sceneLayer.setHeight(30);
        sceneLayer.setColor(Color.RED);
        sceneLayer.setDividerColor(Color.BLACK);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.BOTTOM_CONTROLS_JANK_IMPROVEMENT)
    public void testIsSceneOverlayTreeShowing_FeatureEnabled() {
        mSceneLayer.setIsVisible(false);
        mSceneLayer.setCanShow(true);
        assertTrue(mSceneLayer.isSceneOverlayTreeShowing());

        mSceneLayer.setCanShow(false);
        assertFalse(mSceneLayer.isSceneOverlayTreeShowing());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.BOTTOM_CONTROLS_JANK_IMPROVEMENT)
    public void testIsSceneOverlayTreeShowing_FeatureDisabled() {
        mSceneLayer.setIsVisible(true);
        mSceneLayer.setCanShow(false);
        assertTrue(mSceneLayer.isSceneOverlayTreeShowing());

        mSceneLayer.setIsVisible(false);
        assertFalse(mSceneLayer.isSceneOverlayTreeShowing());
    }
}
