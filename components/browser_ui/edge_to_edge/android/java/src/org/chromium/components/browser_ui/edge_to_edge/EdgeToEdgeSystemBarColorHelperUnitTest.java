// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.edge_to_edge;

import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import android.graphics.Color;
import android.view.View;
import android.view.Window;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit test for {@link EdgeToEdgeSystemBarColorHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = 30)
public class EdgeToEdgeSystemBarColorHelperUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Window mWindow;
    @Mock private View mDecorView;
    @Mock private SystemBarColorHelper mDelegateColorHelper;

    private EdgeToEdgeSystemBarColorHelper mEdgeToEdgeColorHelper;
    private WindowSystemBarColorHelper mWindowHelper;
    private ObservableSupplierImpl<Boolean> mIsEdgeToEdgeSupplier = new ObservableSupplierImpl<>();
    private OneshotSupplierImpl<SystemBarColorHelper> mDelegateHelperSupplier =
            new OneshotSupplierImpl<>();

    @Before
    public void setup() {
        doReturn(mDecorView).when(mWindow).getDecorView();
    }

    @Test
    public void initWhenNotEdgeToEdge_WithoutDelegateHelper_UseWindowHelper() {
        mIsEdgeToEdgeSupplier.set(false);
        initEdgeToEdgeColorHelper();

        mEdgeToEdgeColorHelper.setNavigationBarColor(Color.RED);
        verify(mWindow).setNavigationBarColor(Color.RED);
    }

    @Test
    public void initWhenNotEdgeToEdge_WithDelegateHelper_UseWindowHelper() {
        mIsEdgeToEdgeSupplier.set(false);
        mDelegateHelperSupplier.set(mDelegateColorHelper);
        initEdgeToEdgeColorHelper();

        mEdgeToEdgeColorHelper.setNavigationBarColor(Color.RED);
        verify(mWindow).setNavigationBarColor(Color.RED);
    }

    @Test
    public void initWhenEdgeToEdge_WithoutDelegateHelper_WindowTransparent() {
        mIsEdgeToEdgeSupplier.set(true);
        initEdgeToEdgeColorHelper();

        mEdgeToEdgeColorHelper.setNavigationBarColor(Color.RED);
        verify(mWindow, times(0)).setNavigationBarColor(Color.RED);
    }

    @Test
    public void initWhenEdgeToEdge_WithDelegateHelper_UseDelegateHelper() {
        mIsEdgeToEdgeSupplier.set(true);
        mDelegateHelperSupplier.set(mDelegateColorHelper);
        initEdgeToEdgeColorHelper();

        mEdgeToEdgeColorHelper.setNavigationBarColor(Color.RED);
        verify(mDelegateColorHelper).setNavigationBarColor(Color.RED);
    }

    @Test
    public void switchIntoEdgeToEdge() {
        mDelegateHelperSupplier.set(mDelegateColorHelper);
        initEdgeToEdgeColorHelper();
        mEdgeToEdgeColorHelper.setNavigationBarColor(Color.RED);
        verify(mWindow).setNavigationBarColor(Color.RED);
        verify(mDelegateColorHelper, times(0)).setNavigationBarColor(anyInt());
        verify(mWindow).setNavigationBarContrastEnforced(true);

        clearInvocations(mWindow);
        doReturn(Color.RED).when(mWindow).getNavigationBarColor();

        // Color will switch automatically when edge to edge mode changed.
        mIsEdgeToEdgeSupplier.set(true);
        verify(mDelegateColorHelper).setNavigationBarColor(Color.RED);
        verify(mWindow).setNavigationBarColor(Color.TRANSPARENT);
        verify(mWindow).setNavigationBarContrastEnforced(false);
    }

    @Test
    public void switchOutFromEdgeToEdge() {
        mIsEdgeToEdgeSupplier.set(true);
        mDelegateHelperSupplier.set(mDelegateColorHelper);
        initEdgeToEdgeColorHelper();
        mEdgeToEdgeColorHelper.setNavigationBarColor(Color.RED);
        verify(mDelegateColorHelper).setNavigationBarColor(Color.RED);
        verify(mWindow, times(0)).setNavigationBarColor(Color.TRANSPARENT);
        verify(mWindow).setNavigationBarContrastEnforced(false);

        // Color will switch automatically when edge to edge mode changed.
        clearInvocations(mDelegateColorHelper);
        mIsEdgeToEdgeSupplier.set(false);
        verify(mWindow).setNavigationBarColor(Color.RED);
        verifyNoMoreInteractions(mDelegateColorHelper);
        verify(mWindow).setNavigationBarContrastEnforced(true);
    }

    private void initEdgeToEdgeColorHelper() {
        mEdgeToEdgeColorHelper =
                new EdgeToEdgeSystemBarColorHelper(
                        mWindow, mIsEdgeToEdgeSupplier, mDelegateHelperSupplier);
        mWindowHelper = mEdgeToEdgeColorHelper.getWindowHelperForTesting();
    }
}
