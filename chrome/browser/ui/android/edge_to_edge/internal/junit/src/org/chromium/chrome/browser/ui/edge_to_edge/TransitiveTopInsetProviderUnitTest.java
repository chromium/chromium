// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link TransitiveTopInsetProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TransitiveTopInsetProviderUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TopInsetProvider mTopInsetProvider;
    @Mock private TopInsetProvider.Observer mObserver;
    @Mock private TopInsetProvider.Observer mObserver2;

    private TransitiveTopInsetProvider mTransitiveProvider;

    @Before
    public void setUp() {
        mTransitiveProvider = new TransitiveTopInsetProvider();
    }

    @Test
    public void testAddObserver_immediatelyRegistersWhenProviderAlreadyAvailable() {
        mTransitiveProvider.set(mTopInsetProvider);
        mTransitiveProvider.addObserver(mObserver);
        verify(mTopInsetProvider).addObserver(mObserver);
    }

    @Test
    public void testAddObserver_registersWhenProviderBecomesAvailable() {
        mTransitiveProvider.addObserver(mObserver);
        verify(mTopInsetProvider, never()).addObserver(mObserver);

        mTransitiveProvider.set(mTopInsetProvider);
        verify(mTopInsetProvider).addObserver(mObserver);
    }

    @Test
    public void testAddObserver_registersMultipleObservers() {
        mTransitiveProvider.addObserver(mObserver);
        mTransitiveProvider.addObserver(mObserver2);

        mTransitiveProvider.set(mTopInsetProvider);

        verify(mTopInsetProvider).addObserver(mObserver);
        verify(mTopInsetProvider).addObserver(mObserver2);
    }

    @Test
    public void testRemoveObserver_removesFromPendingList() {
        mTransitiveProvider.addObserver(mObserver);
        mTransitiveProvider.removeObserver(mObserver);

        mTransitiveProvider.set(mTopInsetProvider);
        verify(mTopInsetProvider, never()).addObserver(mObserver);
    }

    @Test
    public void testRemoveObserver_removesFromProvider() {
        mTransitiveProvider.set(mTopInsetProvider);

        mTransitiveProvider.addObserver(mObserver);
        mTransitiveProvider.removeObserver(mObserver);

        verify(mTopInsetProvider).addObserver(mObserver);
        verify(mTopInsetProvider).removeObserver(mObserver);
    }
}
