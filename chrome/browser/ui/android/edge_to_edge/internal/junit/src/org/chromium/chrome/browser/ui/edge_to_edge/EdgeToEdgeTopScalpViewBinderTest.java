// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeTopScalpProperties.ALL_KEYS;
import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeTopScalpProperties.COLOR;
import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeTopScalpProperties.HEIGHT;
import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeTopScalpProperties.IS_VISIBLE;
import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeTopScalpProperties.OFFSET_TAG;
import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeTopScalpProperties.Y_OFFSET;

import android.graphics.Color;

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
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

@RunWith(BaseRobolectricTestRunner.class)
public class EdgeToEdgeTopScalpViewBinderTest {
    private static final int INITIAL_Y_OFFSET = 0;
    private static final int UPDATED_Y_OFFSET = -30;
    private static final int INITIAL_HEIGHT = 60;
    private static final int UPDATED_HEIGHT = 80;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private EdgeToEdgeTopScalpSceneLayer mSceneLayer;

    private PropertyModel mModel;

    @Before
    public void setUp() {
        mModel =
                new PropertyModel.Builder(ALL_KEYS)
                        .with(Y_OFFSET, INITIAL_Y_OFFSET)
                        .with(HEIGHT, INITIAL_HEIGHT)
                        .with(IS_VISIBLE, true)
                        .build();
        PropertyModelChangeProcessor.create(
                mModel, mSceneLayer, EdgeToEdgeTopScalpViewBinder::bind);

        verify(mSceneLayer, atLeastOnce()).setIsVisible(eq(true));
        verify(mSceneLayer).setHeight(INITIAL_HEIGHT);
        clearInvocations(mSceneLayer);
    }

    @Test
    public void testUpdate_YOffset() {
        mModel.set(Y_OFFSET, UPDATED_Y_OFFSET);
        verify(mSceneLayer).setYOffset(UPDATED_Y_OFFSET);

        mModel.set(Y_OFFSET, INITIAL_Y_OFFSET);
        verify(mSceneLayer).setYOffset(INITIAL_Y_OFFSET);
    }

    @Test
    public void testUpdate_Height() {
        mModel.set(HEIGHT, UPDATED_HEIGHT);
        verify(mSceneLayer).setHeight(UPDATED_HEIGHT);
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
    public void testBindCompositorMCP() {
        EdgeToEdgeTopScalpViewBinder.bindCompositorMCP(mModel, mSceneLayer, null);
    }
}
