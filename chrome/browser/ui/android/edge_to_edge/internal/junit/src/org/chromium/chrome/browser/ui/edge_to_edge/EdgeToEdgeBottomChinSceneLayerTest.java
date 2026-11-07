// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.graphics.Color;
import android.graphics.RectF;

import org.jni_zero.JniRawPtr;
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
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayer;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayerJni;

import java.util.HashMap;
import java.util.Map;

@RunWith(BaseRobolectricTestRunner.class)
public class EdgeToEdgeBottomChinSceneLayerTest {
    @Rule public MockitoRule mMockitoJUnit = MockitoJUnit.rule();
    @Mock private SceneLayer.Natives mBaseSceneLayerJni;
    @Mock private EdgeToEdgeBottomChinSceneLayerJni mSceneLayerJni;
    @Mock private Runnable mRequestRenderRunnable;
    private long mNextNativePtr = 123L;
    private final Map<Long, SceneLayer> mLiveLayers = new HashMap<>();
    private JniRawPtr<EdgeToEdgeBottomChinSceneLayer.NativeEdgeToEdgeBottomChinSceneLayer>
            mNativePtr;
    private EdgeToEdgeBottomChinSceneLayer mSceneLayer;

    @Before
    public void setUp() {
        SceneLayerJni.setInstanceForTesting(mBaseSceneLayerJni);
        doAnswer(
                        invocation -> {
                            long rawPtr = mNextNativePtr++;
                            SceneLayer layer = invocation.getArgument(0);
                            layer.setNativePtr(rawPtr);
                            mLiveLayers.put(rawPtr, layer);
                            JniRawPtr<
                                            EdgeToEdgeBottomChinSceneLayer
                                                    .NativeEdgeToEdgeBottomChinSceneLayer>
                                    ptr = JniRawPtr.createForTesting(rawPtr);
                            if (mNativePtr == null) {
                                mNativePtr = ptr;
                            }
                            return ptr;
                        })
                .when(mSceneLayerJni)
                .init(any());
        doAnswer(
                        invocation -> {
                            long rawPtr = invocation.getArgument(0);
                            SceneLayer layer = mLiveLayers.remove(rawPtr);
                            if (layer != null) {
                                layer.setNativePtr(0L);
                            }
                            return null;
                        })
                .when(mBaseSceneLayerJni)
                .destroy(anyLong());
        EdgeToEdgeBottomChinSceneLayerJni.setInstanceForTesting(mSceneLayerJni);
        mSceneLayer = new EdgeToEdgeBottomChinSceneLayer(mRequestRenderRunnable);
    }

    @After
    public void tearDown() {
        mSceneLayer.destroy();
        EdgeToEdgeBottomChinSceneLayerJni.setInstanceForTesting(null);
        SceneLayerJni.setInstanceForTesting(null);
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
                        mNativePtr,
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
        sceneLayer.destroy();
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
