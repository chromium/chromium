// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.edge_to_edge;

import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = 30, shadows = EdgeToEdgeStateProviderUnitTest.ShadowWindowCompat.class)
public class EdgeToEdgeManagerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock EdgeToEdgeStateProvider mEdgeToEdgeStateProvider;

    private EdgeToEdgeManager mEdgeToEdgeManager;

    private EdgeToEdgeManager createEdgeToEdgeManager(boolean shouldDrawEdgeToEdge) {
        return new EdgeToEdgeManager(mEdgeToEdgeStateProvider, shouldDrawEdgeToEdge);
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
    public void testDestroy() {
        mEdgeToEdgeManager = createEdgeToEdgeManager(/* shouldDrawEdgeToEdge= */ true);

        mEdgeToEdgeManager.destroy();
        verify(mEdgeToEdgeStateProvider, atLeastOnce())
                .releaseSetDecorFitsSystemWindowToken(anyInt());
    }
}
