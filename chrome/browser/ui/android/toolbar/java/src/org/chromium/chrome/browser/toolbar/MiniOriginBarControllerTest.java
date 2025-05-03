// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.toolbar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.widget.FrameLayout;

import androidx.coordinatorlayout.widget.CoordinatorLayout;
import androidx.coordinatorlayout.widget.CoordinatorLayout.LayoutParams;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.browser_controls.BrowserControlsSizer;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.omnibox.LocationBar;
import org.chromium.components.browser_ui.widget.TouchEventObserver;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MiniOriginBarControllerTest {
    @Rule public MockitoRule mMockitoJUnit = MockitoJUnit.rule();

    @Mock private ControlContainer mControlContainer;
    @Mock private LocationBar mLocationBar;
    @Mock private View mLocationBarView;
    @Mock private BrowserControlsSizer mBrowserControlsSizer;
    @Captor ArgumentCaptor<TouchEventObserver> mTouchEventObserverCaptor;

    private Context mContext;
    private CoordinatorLayout.LayoutParams mControlContainerLayoutParams =
            new LayoutParams(400, 800);
    private FrameLayout.LayoutParams mLocationBarLayoutParams =
            new FrameLayout.LayoutParams(400, 800, Gravity.TOP);
    private FormFieldFocusedSupplier mIsFormFieldFocused = new FormFieldFocusedSupplier();
    private ToolbarPositionControllerTest.FakeKeyboardVisibilityDelegate
            mKeyboardVisibilityDelegate =
                    new ToolbarPositionControllerTest.FakeKeyboardVisibilityDelegate();
    private MiniOriginBarController mMiniOriginBarController;
    private ObservableSupplierImpl<Boolean> mSuppressToolbarSceneLayerSupplier =
            new ObservableSupplierImpl<>(false);

    @Before
    public void setUp() {
        mContext = ContextUtils.getApplicationContext();
        mControlContainerLayoutParams.gravity = Gravity.TOP;
        doReturn(ControlsPosition.TOP).when(mBrowserControlsSizer).getControlsPosition();
        doReturn(mControlContainerLayoutParams).when(mControlContainer).mutateLayoutParams();
        doReturn(mLocationBarView).when(mLocationBar).getContainerView();
        doReturn(mLocationBarLayoutParams).when(mLocationBarView).getLayoutParams();
        mMiniOriginBarController =
                new MiniOriginBarController(
                        mLocationBar,
                        mIsFormFieldFocused,
                        mKeyboardVisibilityDelegate,
                        mContext,
                        mControlContainer,
                        mSuppressToolbarSceneLayerSupplier,
                        mBrowserControlsSizer);
    }

    @Test
    public void testUpdateMiniOriginBarState() {
        mIsFormFieldFocused.onNodeAttributeUpdated(true, false);
        verify(mLocationBar, never()).setShowOriginOnly(anyBoolean());

        mKeyboardVisibilityDelegate.setVisibilityForTests(true);
        verify(mLocationBar, never()).setShowOriginOnly(anyBoolean());

        doReturn(ControlsPosition.BOTTOM).when(mBrowserControlsSizer).getControlsPosition();
        mMiniOriginBarController.onControlsPositionChanged(ControlsPosition.BOTTOM);
        verify(mLocationBar).setShowOriginOnly(true);
        verify(mLocationBar).setUrlBarUsesSmallText(true);
        assertEquals(
                mContext.getResources().getDimensionPixelSize(R.dimen.mini_origin_bar_height),
                mControlContainerLayoutParams.height);
        assertEquals(Gravity.CENTER, mLocationBarLayoutParams.gravity);

        mKeyboardVisibilityDelegate.setVisibilityForTests(false);
        verify(mLocationBar).setShowOriginOnly(false);
        verify(mLocationBar).setUrlBarUsesSmallText(false);
        assertEquals(LayoutParams.WRAP_CONTENT, mControlContainerLayoutParams.height);
        assertEquals(Gravity.TOP, mLocationBarLayoutParams.gravity);
    }

    @Test
    public void testDestroy() {
        mMiniOriginBarController.destroy();
        mIsFormFieldFocused.onNodeAttributeUpdated(true, false);
        mKeyboardVisibilityDelegate.setVisibilityForTests(true);

        verify(mLocationBar, never()).setShowOriginOnly(true);
    }

    @Test
    public void testTouchEventEndsMiniOriginModeForSession() {
        verify(mControlContainer).addTouchEventObserver(mTouchEventObserverCaptor.capture());
        MotionEvent clickEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, 0, 0);

        TouchEventObserver observer = mTouchEventObserverCaptor.getValue();
        assertFalse(observer.onInterceptTouchEvent(clickEvent));

        mIsFormFieldFocused.onNodeAttributeUpdated(true, false);
        mKeyboardVisibilityDelegate.setVisibilityForTests(true);
        doReturn(ControlsPosition.BOTTOM).when(mBrowserControlsSizer).getControlsPosition();
        mMiniOriginBarController.onControlsPositionChanged(ControlsPosition.BOTTOM);
        verify(mLocationBar).setShowOriginOnly(true);

        assertTrue(observer.onInterceptTouchEvent(clickEvent));

        verify(mLocationBar).setShowOriginOnly(false);
        verify(mLocationBar).setUrlBarUsesSmallText(false);
        assertFalse(observer.onInterceptTouchEvent(clickEvent));

        mIsFormFieldFocused.onNodeAttributeUpdated(false, false);
        // The effect of the click only persists until the "session" ends, e.g. via un-focusing a
        // form or hiding the keyboard.
        mIsFormFieldFocused.onNodeAttributeUpdated(true, false);
        assertTrue(observer.onInterceptTouchEvent(clickEvent));
    }
}
