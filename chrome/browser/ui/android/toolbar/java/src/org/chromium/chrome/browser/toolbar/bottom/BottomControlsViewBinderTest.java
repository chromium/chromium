// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.bottom;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.view.View;
import android.view.ViewGroup;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.resources.Resource;
import org.chromium.ui.resources.dynamics.ViewResourceAdapter;

/** Unit tests for {@link BottomControlsViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BottomControlsViewBinderTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ScrollingBottomViewResourceFrameLayout mRootView;
    @Mock private ScrollingBottomViewSceneLayer mSceneLayer;
    @Mock private ViewResourceAdapter mResourceAdapter;
    @Mock private View mSlotView;
    @Mock private View mShadowView;
    @Mock private ViewGroup.LayoutParams mLayoutParams;
    @Captor private ArgumentCaptor<Callback<Resource>> mResourceCallbackCaptor;

    private PropertyModel mModel;
    private BottomControlsViewBinder.ViewHolder mViewHolder;

    @Before
    public void setUp() {
        doReturn(mResourceAdapter).when(mRootView).getResourceAdapter();
        doReturn(mSlotView).when(mRootView).findViewById(R.id.bottom_container_slot);
        doReturn(mShadowView).when(mRootView).findViewById(R.id.bottom_container_top_shadow);
        doReturn(mLayoutParams).when(mSlotView).getLayoutParams();

        mModel =
                new PropertyModel.Builder(BottomControlsProperties.ALL_KEYS)
                        .with(BottomControlsProperties.ANDROID_VIEW_HEIGHT_NO_PADDING, 80)
                        .with(BottomControlsProperties.BOTTOM_PADDING, 0)
                        .with(BottomControlsProperties.Y_OFFSET, 0)
                        .with(BottomControlsProperties.ANDROID_VIEW_TRANSLATE_Y, 0)
                        .with(BottomControlsProperties.ANDROID_VIEW_VISIBLE, true)
                        .with(BottomControlsProperties.COMPOSITED_VIEW_VISIBLE, true)
                        .build();

        mViewHolder = new BottomControlsViewBinder.ViewHolder(mRootView, mSceneLayer);
        PropertyModelChangeProcessor.create(mModel, mViewHolder, BottomControlsViewBinder::bind);

        assertFalse(mViewHolder.isWaitingForBitmapCapture);
        Mockito.clearInvocations(mRootView, mSceneLayer, mResourceAdapter, mSlotView);
    }

    @Test
    public void testBottomPadding_changed() {
        mModel.set(BottomControlsProperties.BOTTOM_PADDING, 54);

        verify(mRootView).setPadding(anyInt(), anyInt(), anyInt(), eq(54));
        verify(mSceneLayer).setBottomPadding(eq(54));
        verify(mRootView).onModelTokenChange(any());
        verify(mSceneLayer).setIsVisible(eq(false));
        verify(mResourceAdapter).addOnResourceReadyCallback(any());
        assertTrue(mViewHolder.isWaitingForBitmapCapture);
    }

    @Test
    public void testBottomPadding_unchanged() {
        doReturn(54).when(mRootView).getPaddingBottom();
        mModel.set(BottomControlsProperties.BOTTOM_PADDING, 54);

        verify(mRootView, never()).onModelTokenChange(any());
        verify(mSceneLayer, never()).setIsVisible(eq(false));
        assertFalse(mViewHolder.isWaitingForBitmapCapture);
    }

    @Test
    public void testAndroidViewHeightNoPadding_changed() {
        mLayoutParams.height = 80;
        mModel.set(BottomControlsProperties.ANDROID_VIEW_HEIGHT_NO_PADDING, 100);

        verify(mSlotView, atLeastOnce()).getLayoutParams();
        verify(mSceneLayer).setIsVisible(eq(false));
        verify(mResourceAdapter).addOnResourceReadyCallback(any());
        assertTrue(mViewHolder.isWaitingForBitmapCapture);
    }

    @Test
    public void testAndroidViewHeightNoPadding_initialSetup() {
        assertFalse(mViewHolder.isWaitingForBitmapCapture);
    }

    @Test
    public void testYOffset() {
        mModel.set(BottomControlsProperties.Y_OFFSET, 15);
        verify(mSceneLayer).setYOffset(eq(15));
    }

    @Test
    public void testAndroidViewTranslateY() {
        mModel.set(BottomControlsProperties.ANDROID_VIEW_TRANSLATE_Y, 25);
        verify(mRootView).setTranslationY(25f);
    }

    @Test
    public void testCompositedViewVisible_waitingForBitmapCapture_bottomPadding() {
        mModel.set(BottomControlsProperties.BOTTOM_PADDING, 54);
        assertTrue(mViewHolder.isWaitingForBitmapCapture);
        verify(mSceneLayer).setIsVisible(false);
        Mockito.clearInvocations(mSceneLayer);

        // Update COMPOSITED_VIEW_VISIBLE while waiting for bitmap capture.
        mModel.set(BottomControlsProperties.COMPOSITED_VIEW_VISIBLE, true);
        verify(mSceneLayer, never()).setIsVisible(anyBoolean());

        // Trigger resource ready callback.
        verify(mResourceAdapter).addOnResourceReadyCallback(mResourceCallbackCaptor.capture());
        mResourceCallbackCaptor.getValue().onResult(null);

        assertFalse(mViewHolder.isWaitingForBitmapCapture);
        verify(mSceneLayer).setIsVisible(true);
    }

    @Test
    public void testCompositedViewVisible_waitingForBitmapCapture_heightNoPadding() {
        mLayoutParams.height = 80;
        mModel.set(BottomControlsProperties.ANDROID_VIEW_HEIGHT_NO_PADDING, 100);
        assertTrue(mViewHolder.isWaitingForBitmapCapture);
        verify(mSceneLayer).setIsVisible(false);
        Mockito.clearInvocations(mSceneLayer);

        // Update COMPOSITED_VIEW_VISIBLE while waiting for bitmap capture.
        mModel.set(BottomControlsProperties.COMPOSITED_VIEW_VISIBLE, true);
        verify(mSceneLayer, never()).setIsVisible(anyBoolean());

        // Trigger resource ready callback.
        verify(mResourceAdapter).addOnResourceReadyCallback(mResourceCallbackCaptor.capture());
        mResourceCallbackCaptor.getValue().onResult(null);

        assertFalse(mViewHolder.isWaitingForBitmapCapture);
        verify(mSceneLayer).setIsVisible(true);
    }

    @Test
    public void testCompositedViewVisible_notWaitingForBitmapCapture() {
        assertFalse(mViewHolder.isWaitingForBitmapCapture);
        mModel.set(BottomControlsProperties.COMPOSITED_VIEW_VISIBLE, false);
        verify(mSceneLayer).setIsVisible(false);

        mModel.set(BottomControlsProperties.COMPOSITED_VIEW_VISIBLE, true);
        verify(mSceneLayer).setIsVisible(true);
    }

    @Test
    public void testBottomPadding_changed_androidViewNotVisible() {
        mModel.set(BottomControlsProperties.ANDROID_VIEW_VISIBLE, false);
        Mockito.clearInvocations(mSceneLayer, mResourceAdapter);

        mModel.set(BottomControlsProperties.BOTTOM_PADDING, 54);

        verify(mRootView).setPadding(anyInt(), anyInt(), anyInt(), eq(54));
        verify(mSceneLayer).setBottomPadding(eq(54));
        verify(mSceneLayer, never()).setIsVisible(eq(false));
        verify(mResourceAdapter, never()).addOnResourceReadyCallback(any());
        assertFalse(mViewHolder.isWaitingForBitmapCapture);
    }

    @Test
    public void testAndroidViewHeightNoPadding_changed_androidViewNotVisible() {
        mLayoutParams.height = 80;
        mModel.set(BottomControlsProperties.ANDROID_VIEW_VISIBLE, false);
        Mockito.clearInvocations(mSceneLayer, mResourceAdapter);

        mModel.set(BottomControlsProperties.ANDROID_VIEW_HEIGHT_NO_PADDING, 100);

        verify(mSlotView, atLeastOnce()).getLayoutParams();
        verify(mSceneLayer, never()).setIsVisible(eq(false));
        verify(mResourceAdapter, never()).addOnResourceReadyCallback(any());
        assertFalse(mViewHolder.isWaitingForBitmapCapture);
    }

    @Test
    public void testCompositedViewVisible_waitingForBitmapCapture_androidViewNotVisible() {
        mModel.set(BottomControlsProperties.BOTTOM_PADDING, 54);
        assertTrue(mViewHolder.isWaitingForBitmapCapture);
        verify(mSceneLayer).setIsVisible(false);
        Mockito.clearInvocations(mSceneLayer);

        // Hide Android view and show Composited view.
        mModel.set(BottomControlsProperties.ANDROID_VIEW_VISIBLE, false);
        verify(mSceneLayer).setIsVisible(true);
    }
}
