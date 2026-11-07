// Copyright 2026 The Chromium Authors
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
import org.chromium.cc.input.OffsetTag;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayer;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayerJni;

import java.util.HashMap;
import java.util.Map;

@RunWith(BaseRobolectricTestRunner.class)
public class EdgeToEdgeTopScalpSceneLayerTest {
    @Rule public MockitoRule mMockitoJUnit = MockitoJUnit.rule();
    @Mock private Runnable mRequestRenderRunnable;
    @Mock private SceneLayer.Natives mBaseSceneLayerJni;
    @Mock private EdgeToEdgeTopScalpSceneLayerJni mSceneLayerJni;
    @Mock private SceneLayer mContentTree;
    private long mNextNativePtr = 123L;
    private final Map<Long, SceneLayer> mLiveLayers = new HashMap<>();
    private JniRawPtr<EdgeToEdgeTopScalpSceneLayer.NativeEdgeToEdgeTopScalpSceneLayer> mNativePtr;
    private EdgeToEdgeTopScalpSceneLayer mSceneLayer;

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
                                            EdgeToEdgeTopScalpSceneLayer
                                                    .NativeEdgeToEdgeTopScalpSceneLayer>
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
        EdgeToEdgeTopScalpSceneLayerJni.setInstanceForTesting(mSceneLayerJni);
        mSceneLayer = new EdgeToEdgeTopScalpSceneLayer(mRequestRenderRunnable);
    }

    @After
    public void tearDown() {
        mSceneLayer.destroy();
        EdgeToEdgeTopScalpSceneLayerJni.setInstanceForTesting(null);
        SceneLayerJni.setInstanceForTesting(null);
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
        layer.destroy();
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
                        mNativePtr, (int) viewport.width(), 30, Color.RED, 12, offsetTag);
    }

    @Test
    public void testSetContentTree() {
        mSceneLayer.setContentTree(mContentTree);
        verify(mSceneLayerJni).setContentTree(mNativePtr, mContentTree);
    }

    @Test
    public void testIsSceneOverlayTreeShowing() {
        mSceneLayer.setIsVisible(true);
        assertTrue(mSceneLayer.isSceneOverlayTreeShowing());

        mSceneLayer.setIsVisible(false);
        assertFalse(mSceneLayer.isSceneOverlayTreeShowing());
    }
}
