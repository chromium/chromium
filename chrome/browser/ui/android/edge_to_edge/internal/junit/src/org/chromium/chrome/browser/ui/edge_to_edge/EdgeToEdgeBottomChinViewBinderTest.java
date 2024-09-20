// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeBottomChinProperties.CAN_SHOW;
import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeBottomChinProperties.HEIGHT;
import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeBottomChinProperties.Y_OFFSET;

import android.view.View;
import android.view.ViewGroup;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

@RunWith(BaseRobolectricTestRunner.class)
public class EdgeToEdgeBottomChinViewBinderTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private View mAndroidView;
    @Mock private ViewGroup.LayoutParams mAndroidViewLayoutParams;
    @Mock private EdgeToEdgeBottomChinSceneLayer mSceneLayer;

    private PropertyModel mModel;

    @Before
    public void setUp() {
        doReturn(mAndroidViewLayoutParams).when(mAndroidView).getLayoutParams();

        mModel =
                new PropertyModel.Builder(EdgeToEdgeBottomChinProperties.ALL_KEYS)
                        .with(EdgeToEdgeBottomChinProperties.CAN_SHOW, true)
                        .with(EdgeToEdgeBottomChinProperties.Y_OFFSET, 0)
                        .with(EdgeToEdgeBottomChinProperties.HEIGHT, 60)
                        .build();
        PropertyModelChangeProcessor.create(
                mModel,
                new EdgeToEdgeBottomChinViewBinder.ViewHolder(mAndroidView, mSceneLayer),
                EdgeToEdgeBottomChinViewBinder::bind);

        verify(mSceneLayer, atLeastOnce()).setIsVisible(eq(true));
        verify(mAndroidView, atLeastOnce()).setVisibility(eq(View.VISIBLE));
        clearInvocations(mSceneLayer, mAndroidView);
    }

    @Test
    public void testUpdate_YOffset() {
        // Set the y-offset to partial height.
        mModel.set(Y_OFFSET, mModel.get(HEIGHT) / 2);
        verify(mSceneLayer).setYOffset(mModel.get(HEIGHT) / 2);
        verify(mAndroidView, never()).setVisibility(eq(View.GONE));
        verify(mSceneLayer, never()).setIsVisible(eq(false));

        // Set the y-offset to full height.
        mModel.set(Y_OFFSET, mModel.get(HEIGHT));
        verify(mSceneLayer).setYOffset(mModel.get(HEIGHT));
        verify(mAndroidView, atLeastOnce()).setVisibility(eq(View.GONE));
        verify(mSceneLayer, atLeastOnce()).setIsVisible(eq(false));

        clearInvocations(mSceneLayer);

        // Clear the y-offset.
        mModel.set(Y_OFFSET, 0);
        verify(mSceneLayer).setYOffset(0);
        verify(mAndroidView, atLeastOnce()).setVisibility(eq(View.VISIBLE));
        verify(mSceneLayer, atLeastOnce()).setIsVisible(eq(true));
    }

    @Test
    public void testUpdate_Height() {
        mModel.set(HEIGHT, 0);
        verify(mAndroidView, atLeastOnce()).setVisibility(eq(View.GONE));
        verify(mSceneLayer, atLeastOnce()).setIsVisible(eq(false));

        mModel.set(HEIGHT, 60);
        verify(mAndroidView, atLeastOnce()).setVisibility(eq(View.VISIBLE));
        verify(mSceneLayer, atLeastOnce()).setIsVisible(eq(true));
    }

    @Test
    public void testUpdate_CanShow() {
        mModel.set(CAN_SHOW, false);
        verify(mAndroidView, atLeastOnce()).setVisibility(eq(View.GONE));
        verify(mSceneLayer, atLeastOnce()).setIsVisible(eq(false));

        mModel.set(CAN_SHOW, true);
        verify(mAndroidView, atLeastOnce()).setVisibility(eq(View.VISIBLE));
        verify(mSceneLayer, atLeastOnce()).setIsVisible(eq(true));
    }
}
