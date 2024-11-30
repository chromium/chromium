// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.edge_to_edge;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.view.Window;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;

@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = 30, shadows = EdgeToEdgeStateProviderUnitTest.ShadowWindowCompat.class)
public class EdgeToEdgeManagerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock Activity mActivity;
    @Mock Window mWindow;
    @Mock EdgeToEdgeStateProvider mEdgeToEdgeStateProvider;
    @Mock SystemBarColorHelper mSystemBarColorHelper;

    private EdgeToEdgeManager mEdgeToEdgeManager;
    private OneshotSupplierImpl<SystemBarColorHelper> mSystemBarColorHelperSupplier;

    @Before
    public void setup() {
        doReturn(mWindow).when(mActivity).getWindow();

        mSystemBarColorHelperSupplier = new OneshotSupplierImpl<>();
        mSystemBarColorHelperSupplier.set(mSystemBarColorHelper);
    }

    private EdgeToEdgeManager createEdgeToEdgeManager(boolean shouldDrawEdgeToEdge) {
        return new EdgeToEdgeManager(
                mActivity,
                mEdgeToEdgeStateProvider,
                mSystemBarColorHelperSupplier,
                shouldDrawEdgeToEdge);
    }

    @Test
    public void testCreateEdgeToEdgeManager() {
        mEdgeToEdgeManager = createEdgeToEdgeManager(/* shouldDrawEdgeToEdge= */ true);
        assertNotNull(mEdgeToEdgeManager.getEdgeToEdgeStateProvider());
        assertNotNull(mEdgeToEdgeManager.getEdgeToEdgeSystemBarColorHelper());
        assertEquals(
                mSystemBarColorHelper,
                mEdgeToEdgeManager
                        .getEdgeToEdgeSystemBarColorHelper()
                        .getEdgeToEdgeDelegateHelperForTesting());
    }

    @Test
    public void testShouldDrawEdgeToEdge() {
        mEdgeToEdgeManager = createEdgeToEdgeManager(/* shouldDrawEdgeToEdge= */ true);
        verify(mEdgeToEdgeStateProvider, atLeastOnce()).acquireSetDecorFitsSystemWindowToken();
    }

    @Test
    public void testShouldNotDrawEdgeToEdge() {
        mEdgeToEdgeManager = createEdgeToEdgeManager(/* shouldDrawEdgeToEdge= */ false);
        verify(mEdgeToEdgeStateProvider, never()).acquireSetDecorFitsSystemWindowToken();
    }

    @Test
    public void testEdgeToEdgeSystemBarColorHelperInitializedAfterCreation() {
        OneshotSupplierImpl<SystemBarColorHelper> systemBarColorHelperSupplier =
                new OneshotSupplierImpl<>();
        EdgeToEdgeManager edgeToEdgeManager =
                new EdgeToEdgeManager(
                        mActivity,
                        mEdgeToEdgeStateProvider,
                        systemBarColorHelperSupplier,
                        /* shouldDrawEdgeToEdge= */ true);

        assertNull(
                edgeToEdgeManager
                        .getEdgeToEdgeSystemBarColorHelper()
                        .getEdgeToEdgeDelegateHelperForTesting());

        systemBarColorHelperSupplier.set(mSystemBarColorHelper);
        assertEquals(
                mSystemBarColorHelper,
                edgeToEdgeManager
                        .getEdgeToEdgeSystemBarColorHelper()
                        .getEdgeToEdgeDelegateHelperForTesting());
    }

    @Test
    public void testDestroy() {
        mEdgeToEdgeManager = createEdgeToEdgeManager(/* shouldDrawEdgeToEdge= */ true);

        mEdgeToEdgeManager.destroy();
        verify(mEdgeToEdgeStateProvider, atLeastOnce())
                .releaseSetDecorFitsSystemWindowToken(anyInt());
    }
}
