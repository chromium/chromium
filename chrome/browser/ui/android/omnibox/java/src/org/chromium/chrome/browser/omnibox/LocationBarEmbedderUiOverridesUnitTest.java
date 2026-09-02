// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ui.side_ui.SideUiStateProvider;

/** Unit tests for {@link LocationBarEmbedderUiOverrides}. */
@RunWith(BaseRobolectricTestRunner.class)
public class LocationBarEmbedderUiOverridesUnitTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private Callback<SideUiStateProvider> mObserver1;
    @Mock private Callback<SideUiStateProvider> mObserver2;
    @Mock private SideUiStateProvider mSideUiStateProvider;
    @Mock private SideUiStateProvider mSideUiStateProvider2;

    private LocationBarEmbedderUiOverrides mUiOverrides;

    @Before
    public void setUp() {
        mUiOverrides = new LocationBarEmbedderUiOverrides();
    }

    @Test
    public void testSideUiStateProviderSupplier_notifiedOnSet() {
        assertNull(mUiOverrides.getSideUiStateProvider());
        mUiOverrides.getSideUiStateProviderSupplier().addSyncObserverAndCallIfNonNull(mObserver1);
        verify(mObserver1, never()).onResult(any());

        mUiOverrides.setSideUiStateProvider(mSideUiStateProvider);
        verify(mObserver1).onResult(mSideUiStateProvider);
        assertEquals(mSideUiStateProvider, mUiOverrides.getSideUiStateProvider());
    }

    @Test
    public void testSideUiStateProviderSupplier_notifiedImmediatelyIfAlreadySet() {
        mUiOverrides.setSideUiStateProvider(mSideUiStateProvider);

        mUiOverrides.getSideUiStateProviderSupplier().addSyncObserverAndCallIfNonNull(mObserver1);
        verify(mObserver1).onResult(mSideUiStateProvider);
    }

    @Test
    public void testSideUiStateProviderSupplier_removeObserver() {
        mUiOverrides.getSideUiStateProviderSupplier().addSyncObserverAndCallIfNonNull(mObserver1);
        mUiOverrides.getSideUiStateProviderSupplier().removeObserver(mObserver1);

        mUiOverrides.setSideUiStateProvider(mSideUiStateProvider);
        verify(mObserver1, never()).onResult(any());
    }

    @Test
    public void testSideUiStateProviderSupplier_multipleObservers() {
        mUiOverrides.getSideUiStateProviderSupplier().addSyncObserverAndCallIfNonNull(mObserver1);
        mUiOverrides.getSideUiStateProviderSupplier().addSyncObserverAndCallIfNonNull(mObserver2);

        mUiOverrides.setSideUiStateProvider(mSideUiStateProvider);
        verify(mObserver1).onResult(mSideUiStateProvider);
        verify(mObserver2).onResult(mSideUiStateProvider);

        mUiOverrides.getSideUiStateProviderSupplier().removeObserver(mObserver1);

        mUiOverrides.setSideUiStateProvider(mSideUiStateProvider2);
        verify(mObserver1).onResult(any());
        verify(mObserver2).onResult(mSideUiStateProvider2);
    }

    @Test
    public void testIsFullWidthExpansionAllowed() {
        assertTrue(mUiOverrides.isFullWidthExpansionAllowed());

        mUiOverrides.setIsFullWidthExpansionAllowedSupplier(() -> false);
        assertFalse(mUiOverrides.isFullWidthExpansionAllowed());

        mUiOverrides.setIsFullWidthExpansionAllowedSupplier(() -> true);
        assertTrue(mUiOverrides.isFullWidthExpansionAllowed());
    }
}
