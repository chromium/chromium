// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.bottom;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.view.View;
import android.widget.FrameLayout;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.resources.dynamics.ViewResourceAdapter;

/** Unit tests for {@link BottomControlsViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BottomControlsViewBinderTest {
    /**
     * Returns a mock {@link ViewResourceAdapter}, since the real adapter's cached bitmap is not
     * observable from tests.
     */
    private static class TestBottomView extends ScrollingBottomViewResourceFrameLayout {
        private final ViewResourceAdapter mResourceAdapter;

        TestBottomView(Context context, ViewResourceAdapter resourceAdapter) {
            super(context, null);
            mResourceAdapter = resourceAdapter;
        }

        @Override
        public ViewResourceAdapter getResourceAdapter() {
            return mResourceAdapter;
        }
    }

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ScrollingBottomViewSceneLayer mSceneLayer;
    @Mock private ViewResourceAdapter mResourceAdapter;

    private ScrollingBottomViewResourceFrameLayout mRootView;
    private View mSlotView;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        Context context = ApplicationProvider.getApplicationContext();
        mRootView = new TestBottomView(context, mResourceAdapter);
        mSlotView = new FrameLayout(context);
        mSlotView.setId(R.id.bottom_container_slot);
        mRootView.addView(mSlotView, new FrameLayout.LayoutParams(0, 0));
        View shadowView = new View(context);
        shadowView.setId(R.id.bottom_container_top_shadow);
        mRootView.addView(shadowView);

        mModel =
                new PropertyModel.Builder(BottomControlsProperties.ALL_KEYS)
                        .with(BottomControlsProperties.ANDROID_VIEW_HEIGHT_NO_PADDING, 80)
                        .with(BottomControlsProperties.BOTTOM_PADDING, 0)
                        .with(BottomControlsProperties.Y_OFFSET, 0)
                        .with(BottomControlsProperties.ANDROID_VIEW_TRANSLATE_Y, 0)
                        .with(BottomControlsProperties.ANDROID_VIEW_VISIBLE, true)
                        .with(BottomControlsProperties.COMPOSITED_VIEW_VISIBLE, true)
                        .build();

        PropertyModelChangeProcessor.create(
                mModel,
                new BottomControlsViewBinder.ViewHolder(mRootView, mSceneLayer),
                BottomControlsViewBinder::bind);

        Mockito.clearInvocations(mSceneLayer, mResourceAdapter);
    }

    @Test
    public void testBottomPadding_changed() {
        mModel.set(BottomControlsProperties.BOTTOM_PADDING, 54);

        assertEquals(54, mRootView.getPaddingBottom());
        verify(mSceneLayer).setBottomPadding(eq(54));
    }

    @Test
    public void testBottomPadding_unchanged() {
        mRootView.setPadding(0, 0, 0, 54);
        mModel.set(BottomControlsProperties.BOTTOM_PADDING, 54);

        verify(mSceneLayer, never()).setBottomPadding(anyInt());
    }

    @Test
    public void testAndroidViewHeightNoPadding_changed() {
        assertEquals(80, mSlotView.getLayoutParams().height);
        mModel.set(BottomControlsProperties.ANDROID_VIEW_HEIGHT_NO_PADDING, 100);

        assertEquals(100, mSlotView.getLayoutParams().height);
        verify(mSceneLayer).setContentHeight(100);
    }

    @Test
    public void testYOffset() {
        mModel.set(BottomControlsProperties.Y_OFFSET, 15);
        verify(mSceneLayer).setYOffset(eq(15));
    }

    @Test
    public void testAndroidViewTranslateY() {
        mModel.set(BottomControlsProperties.ANDROID_VIEW_TRANSLATE_Y, 25);
        assertEquals(25f, mRootView.getTranslationY(), 0f);
    }

    @Test
    public void testCompositedViewVisible_changed() {
        mModel.set(BottomControlsProperties.COMPOSITED_VIEW_VISIBLE, false);
        verify(mSceneLayer).setIsVisible(false);

        mModel.set(BottomControlsProperties.COMPOSITED_VIEW_VISIBLE, true);
        verify(mSceneLayer).setIsVisible(true);
    }

    @Test
    public void testAndroidViewVisible_changed() {
        mModel.set(BottomControlsProperties.ANDROID_VIEW_VISIBLE, false);
        assertEquals(View.INVISIBLE, mRootView.getVisibility());

        mModel.set(BottomControlsProperties.ANDROID_VIEW_VISIBLE, true);
        assertEquals(View.VISIBLE, mRootView.getVisibility());
    }

    @Test
    public void testDropCachedBitmap_whenBothHidden() {
        mModel.set(BottomControlsProperties.ANDROID_VIEW_VISIBLE, false);
        verify(mResourceAdapter, never()).dropCachedBitmap();

        mModel.set(BottomControlsProperties.COMPOSITED_VIEW_VISIBLE, false);
        verify(mResourceAdapter).dropCachedBitmap();
    }
}
