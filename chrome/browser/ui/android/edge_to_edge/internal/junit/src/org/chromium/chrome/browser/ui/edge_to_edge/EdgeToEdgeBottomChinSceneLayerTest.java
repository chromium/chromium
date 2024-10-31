// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.graphics.Color;
import android.graphics.RectF;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class EdgeToEdgeBottomChinSceneLayerTest {
    @Rule public MockitoRule mMockitoJUnit = MockitoJUnit.rule();
    @Rule public JniMocker mJniMocker = new JniMocker();
    @Mock private Runnable mRequestRenderRunnable;
    @Mock private EdgeToEdgeBottomChinSceneLayerJni mSceneLayerJni;
    private EdgeToEdgeBottomChinSceneLayer mSceneLayer;

    @Before
    public void setUp() {
        doReturn(123L).when(mSceneLayerJni).init(any());
        mJniMocker.mock(EdgeToEdgeBottomChinSceneLayerJni.TEST_HOOKS, mSceneLayerJni);
        mSceneLayer = new EdgeToEdgeBottomChinSceneLayer(mRequestRenderRunnable);
    }

    @Test
    public void testUpdatesRequestRender() {
        mSceneLayer.setIsVisible(true);
        verify(mRequestRenderRunnable).run();

        mSceneLayer.setHeight(30);
        verify(mRequestRenderRunnable, times(2)).run();

        mSceneLayer.setColor(Color.RED);
        verify(mRequestRenderRunnable, times(3)).run();

        mSceneLayer.setDividerColor(Color.RED);
        verify(mRequestRenderRunnable, times(4)).run();
    }

    @Test
    public void testGetUpdatedSceneOverlayTree() {
        mSceneLayer.setYOffset(12);
        mSceneLayer.setIsVisible(true);
        mSceneLayer.setHeight(30);
        mSceneLayer.setColor(Color.RED);
        mSceneLayer.setDividerColor(Color.BLACK);

        RectF viewport = new RectF(0, 0, 100, 400);
        mSceneLayer.getUpdatedSceneOverlayTree(viewport, viewport, null, 0);
        verify(mSceneLayerJni)
                .updateEdgeToEdgeBottomChinLayer(
                        123,
                        (int) viewport.width(),
                        30,
                        Color.RED,
                        Color.BLACK,
                        viewport.height() + 12);
    }
}
