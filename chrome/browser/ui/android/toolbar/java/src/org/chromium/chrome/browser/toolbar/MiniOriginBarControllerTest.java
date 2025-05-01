// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.toolbar;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.content.Context;

import androidx.coordinatorlayout.widget.CoordinatorLayout;
import androidx.coordinatorlayout.widget.CoordinatorLayout.LayoutParams;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
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

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MiniOriginBarControllerTest {
    @Rule public MockitoRule mMockitoJUnit = MockitoJUnit.rule();

    @Mock private ControlContainer mControlContainer;
    @Mock private LocationBar mLocationBar;
    @Mock private BrowserControlsSizer mBrowserControlsSizer;

    private Context mContext;
    private CoordinatorLayout.LayoutParams mLayoutParams = new LayoutParams(400, 800);
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
        doReturn(ControlsPosition.TOP).when(mBrowserControlsSizer).getControlsPosition();
        doReturn(mLayoutParams).when(mControlContainer).mutateLayoutParams();
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
                mLayoutParams.height);

        mKeyboardVisibilityDelegate.setVisibilityForTests(false);
        verify(mLocationBar).setShowOriginOnly(false);
        verify(mLocationBar).setUrlBarUsesSmallText(false);
        assertEquals(LayoutParams.WRAP_CONTENT, mLayoutParams.height);
    }

    @Test
    public void testDestroy() {
        mMiniOriginBarController.destroy();
        mIsFormFieldFocused.onNodeAttributeUpdated(true, false);
        mKeyboardVisibilityDelegate.setVisibilityForTests(true);

        verify(mLocationBar, never()).setShowOriginOnly(true);
    }
}
