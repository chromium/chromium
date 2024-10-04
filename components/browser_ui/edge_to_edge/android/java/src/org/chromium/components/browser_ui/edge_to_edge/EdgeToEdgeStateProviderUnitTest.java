// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.edge_to_edge;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.view.Window;

import androidx.annotation.NonNull;
import androidx.core.view.WindowCompat;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.edge_to_edge.EdgeToEdgeStateProviderUnitTest.ShadowWindowCompat;

@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = 30, shadows = ShadowWindowCompat.class)
@SuppressWarnings("NewApi") // Suppress warning of setDecorFitsSystemWindows
public class EdgeToEdgeStateProviderUnitTest {

    @Implements(WindowCompat.class)
    public static class ShadowWindowCompat {
        @Implementation
        protected static void setDecorFitsSystemWindows(
                @NonNull Window window, boolean decorFitsSystemWindows) {

            window.setDecorFitsSystemWindows(decorFitsSystemWindows);
        }
    }

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock Window mWindow;

    private EdgeToEdgeStateProvider mEdgeToEdgeStateProvider;

    @Before
    public void setup() {
        mEdgeToEdgeStateProvider = new EdgeToEdgeStateProvider(mWindow);
    }

    @Test
    public void acquireAndRelease() {
        int token1 = mEdgeToEdgeStateProvider.acquireSetDecorFitsSystemWindowToken();
        verify(mWindow).setDecorFitsSystemWindows(false);
        assertTrue("Should start drawing edge to edge", mEdgeToEdgeStateProvider.get());

        mEdgeToEdgeStateProvider.releaseSetDecorFitsSystemWindowToken(token1);
        verify(mWindow).setDecorFitsSystemWindows(true);
        assertFalse("Edge to edge released.", mEdgeToEdgeStateProvider.get());
    }

    @Test
    public void acquireAndReleaseByMultipleClients() {
        int token1 = mEdgeToEdgeStateProvider.acquireSetDecorFitsSystemWindowToken();
        verify(mWindow).setDecorFitsSystemWindows(false);
        assertTrue("Drawing edge to edge.", mEdgeToEdgeStateProvider.get());

        int token2 = mEdgeToEdgeStateProvider.acquireSetDecorFitsSystemWindowToken();

        mEdgeToEdgeStateProvider.releaseSetDecorFitsSystemWindowToken(token1);
        verify(mWindow, times(0)).setDecorFitsSystemWindows(true);
        assertTrue("Token not empty, still drawing edge to edge.", mEdgeToEdgeStateProvider.get());

        mEdgeToEdgeStateProvider.releaseSetDecorFitsSystemWindowToken(token2);
        verify(mWindow).setDecorFitsSystemWindows(true);
        assertFalse("All token released, exit edge to edge.", mEdgeToEdgeStateProvider.get());
    }
}
