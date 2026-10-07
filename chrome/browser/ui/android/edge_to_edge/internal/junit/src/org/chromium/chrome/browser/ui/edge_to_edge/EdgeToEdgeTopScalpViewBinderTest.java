// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeTopScalpProperties.ALL_KEYS;
import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeTopScalpProperties.CAN_SHOW;
import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeTopScalpProperties.COLOR;
import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeTopScalpProperties.HEIGHT;
import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeTopScalpProperties.IS_VISIBLE;
import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeTopScalpProperties.OFFSET_TAG;
import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeTopScalpProperties.Y_OFFSET;

import android.graphics.Color;
import android.view.View;
import android.view.ViewGroup;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.cc.input.OffsetTag;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

@RunWith(BaseRobolectricTestRunner.class)
public class EdgeToEdgeTopScalpViewBinderTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private View mAndroidView;
    @Mock private EdgeToEdgeTopScalpSceneLayer mSceneLayer;

    private PropertyModel mModel;

    @Before
    public void setUp() {
        mAndroidView = new View(ContextUtils.getApplicationContext());
        mAndroidView.setLayoutParams(new ViewGroup.LayoutParams(100, 0));

        mModel =
                new PropertyModel.Builder(ALL_KEYS)
                        .with(CAN_SHOW, true)
                        .with(Y_OFFSET, 0)
                        .with(HEIGHT, 60)
                        .with(IS_VISIBLE, true)
                        .build();
        PropertyModelChangeProcessor.create(
                mModel,
                new EdgeToEdgeTopScalpViewBinder.ViewHolder(mAndroidView, mSceneLayer),
                EdgeToEdgeTopScalpViewBinder::bind);

        verify(mSceneLayer, atLeastOnce()).setIsVisible(eq(true));
        assertEquals(View.VISIBLE, mAndroidView.getVisibility());
        assertEquals(60, mAndroidView.getLayoutParams().height);
        clearInvocations(mSceneLayer);
    }

    @Test
    public void testUpdate_YOffset() {
        mModel.set(Y_OFFSET, -30);
        verify(mSceneLayer).setYOffset(-30);
        assertEquals(-30f, mAndroidView.getTranslationY(), 0.01f);

        mModel.set(Y_OFFSET, 0);
        verify(mSceneLayer).setYOffset(0);
        assertEquals(0f, mAndroidView.getTranslationY(), 0.01f);
    }

    @Test
    public void testUpdate_Height() {
        mModel.set(HEIGHT, 80);
        verify(mSceneLayer).setHeight(80);
        assertEquals(80, mAndroidView.getLayoutParams().height);
        clearInvocations(mSceneLayer);

        // When LayoutParams is null (e.g. a View before layout attachment), binding height still
        // updates SceneLayer without crashing.
        View unattachedView = new View(ContextUtils.getApplicationContext());
        assertNull(unattachedView.getLayoutParams());
        EdgeToEdgeTopScalpViewBinder.bind(
                mModel,
                new EdgeToEdgeTopScalpViewBinder.ViewHolder(unattachedView, mSceneLayer),
                HEIGHT);
        assertNull(unattachedView.getLayoutParams());
        verify(mSceneLayer).setHeight(80);
    }

    @Test
    public void testUpdate_CanShow() {
        mModel.set(CAN_SHOW, false);
        assertEquals(View.GONE, mAndroidView.getVisibility());

        mModel.set(CAN_SHOW, true);
        assertEquals(View.VISIBLE, mAndroidView.getVisibility());
    }

    @Test
    public void testUpdate_Color() {
        mModel.set(COLOR, Color.RED);
        verify(mSceneLayer).setColor(Color.RED);
    }

    @Test
    public void testUpdate_OffsetTag() {
        OffsetTag offsetTag = new OffsetTag(Token.EMPTY);
        mModel.set(OFFSET_TAG, offsetTag);
        verify(mSceneLayer).setOffsetTag(offsetTag);
    }

    @Test
    public void testUpdate_IsVisible() {
        mModel.set(IS_VISIBLE, false);
        verify(mSceneLayer).setIsVisible(false);

        mModel.set(IS_VISIBLE, true);
        verify(mSceneLayer).setIsVisible(true);
    }

    @Test
    public void testUpdate_NullAndroidView() {
        PropertyModel model = new PropertyModel.Builder(ALL_KEYS).build();
        PropertyModelChangeProcessor.create(
                model,
                new EdgeToEdgeTopScalpViewBinder.ViewHolder(/* androidView= */ null, mSceneLayer),
                EdgeToEdgeTopScalpViewBinder::bind);
        clearInvocations(mSceneLayer);

        model.set(Y_OFFSET, -20);
        verify(mSceneLayer).setYOffset(-20);

        model.set(HEIGHT, 40);
        verify(mSceneLayer).setHeight(40);

        model.set(CAN_SHOW, true);
    }

    @Test
    public void testBindCompositorMCP() {
        EdgeToEdgeTopScalpViewBinder.bindCompositorMCP(mModel, mSceneLayer, null);
    }
}
