// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.edge_to_edge;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.graphics.Color;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.edge_to_edge.layout.EdgeToEdgeLayoutCoordinator;

import java.util.concurrent.TimeUnit;

/** Unit test for {@link TabbedSystemBarColorHelper} */
@RunWith(BaseRobolectricTestRunner.class)
public class TabbedSystemBarColorHelperUnitTest {
    private static final int CALLBACK_DURATION_MS = 100;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private EdgeToEdgeLayoutCoordinator mEdgeToEdgeLayoutCoordinator;
    @Mock private SystemBarColorHelper mEdgeToEdgeBottomChinCoordinator;

    private TabbedSystemBarColorHelper mTabbedSystemBarColorHelper;
    private final OneshotSupplierImpl<SystemBarColorHelper>
            mEdgeToEdgeBottomChinCoordinatorSupplier = new OneshotSupplierImpl<>();

    @Before
    public void setup() {
        mTabbedSystemBarColorHelper =
                new TabbedSystemBarColorHelper(
                        mEdgeToEdgeLayoutCoordinator, mEdgeToEdgeBottomChinCoordinatorSupplier);
    }

    @Test
    public void testEdgeToEdgeBottomChinCoordinatorIsAvailable() {
        mEdgeToEdgeBottomChinCoordinatorSupplier.set(mEdgeToEdgeBottomChinCoordinator);
        mTabbedSystemBarColorHelper.setStatusBarColor(Color.BLUE);
        verify(mEdgeToEdgeLayoutCoordinator).setStatusBarColor(Color.BLUE);

        mTabbedSystemBarColorHelper.setNavigationBarColor(Color.BLUE);
        verify(mEdgeToEdgeBottomChinCoordinator).setNavigationBarColor(Color.BLUE);
        verify(mEdgeToEdgeLayoutCoordinator).setNavigationBarColor(Color.TRANSPARENT);
        verify(mEdgeToEdgeLayoutCoordinator, never()).setNavigationBarColor(Color.BLUE);
    }

    @Test
    public void testEdgeToEdgeBottomChinCoordinatorIsNull() {
        mTabbedSystemBarColorHelper.setStatusBarColor(Color.BLUE);
        verify(mEdgeToEdgeLayoutCoordinator).setStatusBarColor(Color.BLUE);

        mTabbedSystemBarColorHelper.setNavigationBarColor(Color.BLUE);
        verify(mEdgeToEdgeLayoutCoordinator).setNavigationBarColor(Color.BLUE);
    }

    @Test
    public void testEdgeToEdgeBottomChinCoordinatorInitiallyUnavailable() {
        mTabbedSystemBarColorHelper.setStatusBarColor(Color.BLUE);
        verify(mEdgeToEdgeLayoutCoordinator).setStatusBarColor(Color.BLUE);

        mTabbedSystemBarColorHelper.setNavigationBarColor(Color.BLUE);
        verify(mEdgeToEdgeLayoutCoordinator).setNavigationBarColor(Color.BLUE);

        mEdgeToEdgeBottomChinCoordinatorSupplier.set(mEdgeToEdgeBottomChinCoordinator);
        ShadowLooper.idleMainLooper(CALLBACK_DURATION_MS, TimeUnit.MILLISECONDS);
        verify(mEdgeToEdgeBottomChinCoordinator).setNavigationBarColor(Color.BLUE);
        verify(mEdgeToEdgeLayoutCoordinator).setNavigationBarColor(Color.TRANSPARENT);
    }
}
