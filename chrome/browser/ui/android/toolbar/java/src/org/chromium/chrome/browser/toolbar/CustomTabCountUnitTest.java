// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

/** Unit tests for {@link CustomTabCount}. */
@RunWith(BaseRobolectricTestRunner.class)
public class CustomTabCountUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private TabModelSelector mTabModelSelector;
    private final ObservableSupplierImpl<TabModelSelector> mTabModelSelectorSupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<Integer> mTabModelSelectorTabCountSupplier =
            new ObservableSupplierImpl<>();
    private CustomTabCount mCustomTabCount;

    @Before
    public void setUp() {
        mTabModelSelectorTabCountSupplier.set(0);
        when(mTabModelSelector.getCurrentModelTabCountSupplier())
                .thenReturn(mTabModelSelectorTabCountSupplier);
        mCustomTabCount = new CustomTabCount(mTabModelSelectorSupplier);
        mTabModelSelectorSupplier.set(mTabModelSelector);
    }

    @Test
    public void testTabCountSupplier() {
        mTabModelSelectorTabCountSupplier.set(1);
        assertEquals(1, (int) mCustomTabCount.get());
        assertFalse(mCustomTabCount.hasTokensForTesting());

        mTabModelSelectorTabCountSupplier.set(10);
        assertEquals(10, (int) mCustomTabCount.get());
        assertFalse(mCustomTabCount.hasTokensForTesting());

        mTabModelSelectorTabCountSupplier.set(6);
        assertEquals(6, (int) mCustomTabCount.get());
        assertFalse(mCustomTabCount.hasTokensForTesting());
    }

    @Test
    public void testCustomTabCount() {
        int token = mCustomTabCount.setCount(4);
        assertEquals(4, (int) mCustomTabCount.get());
        assertTrue(mCustomTabCount.hasTokensForTesting());

        mTabModelSelectorTabCountSupplier.set(10);
        assertEquals(4, (int) mCustomTabCount.get());
        assertTrue(mCustomTabCount.hasTokensForTesting());

        mCustomTabCount.releaseCount(token);
        assertEquals(10, (int) mCustomTabCount.get());
        assertFalse(mCustomTabCount.hasTokensForTesting());
    }

    @Test
    public void testMultipleTokens() {
        int token1 = mCustomTabCount.setCount(4);
        assertEquals(4, (int) mCustomTabCount.get());
        assertTrue(mCustomTabCount.hasTokensForTesting());

        int token2 = mCustomTabCount.setCount(5);
        assertEquals(5, (int) mCustomTabCount.get());
        assertTrue(mCustomTabCount.hasTokensForTesting());

        mTabModelSelectorTabCountSupplier.set(10);
        assertEquals(5, (int) mCustomTabCount.get());
        assertTrue(mCustomTabCount.hasTokensForTesting());

        mCustomTabCount.releaseCount(token1);
        assertEquals(5, (int) mCustomTabCount.get());
        assertTrue(mCustomTabCount.hasTokensForTesting());

        mCustomTabCount.releaseCount(token2);
        assertEquals(10, (int) mCustomTabCount.get());
        assertFalse(mCustomTabCount.hasTokensForTesting());
    }
}
