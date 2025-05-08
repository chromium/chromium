// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link CustomTabCount}. */
@RunWith(BaseRobolectricTestRunner.class)
public class CustomTabCountUnitTest {
    private final ObservableSupplierImpl<Integer> mTabModelSelectorTabCountSupplier =
            new ObservableSupplierImpl<>();
    private CustomTabCount mCustomTabCount;

    @Before
    public void setUp() {
        mTabModelSelectorTabCountSupplier.set(0);
        mCustomTabCount = new CustomTabCount(mTabModelSelectorTabCountSupplier);
    }

    @Test
    public void testTabCountSupplier() {
        mTabModelSelectorTabCountSupplier.set(1);
        assertEquals(1, (int) mCustomTabCount.get());
        assertFalse(mCustomTabCount.getIsCustomForTesting());

        mTabModelSelectorTabCountSupplier.set(10);
        assertEquals(10, (int) mCustomTabCount.get());
        assertFalse(mCustomTabCount.getIsCustomForTesting());

        mTabModelSelectorTabCountSupplier.set(6);
        assertEquals(6, (int) mCustomTabCount.get());
        assertFalse(mCustomTabCount.getIsCustomForTesting());
    }

    @Test
    public void testCustomTabCount() {
        mCustomTabCount.set(4);
        assertEquals(4, (int) mCustomTabCount.get());
        assertTrue(mCustomTabCount.getIsCustomForTesting());

        mTabModelSelectorTabCountSupplier.set(10);
        assertEquals(4, (int) mCustomTabCount.get());
        assertTrue(mCustomTabCount.getIsCustomForTesting());

        mCustomTabCount.release();
        assertEquals(10, (int) mCustomTabCount.get());
        assertFalse(mCustomTabCount.getIsCustomForTesting());
    }
}
