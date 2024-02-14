// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/**
 * Helper class used to wrap around an ObservableSupplierImpl and provide debug info if supplier is
 * modified.
 */
public class TabStripHeightSupplier extends ObservableSupplierImpl<Integer> {
    public TabStripHeightSupplier(int initValue) {
        super(initValue);
    }

    /**
     * Set the new supplier value. Has to be used when the supplier is enabled.
     *
     * @param newHeight The new supplier value.
     */
    @Override
    public void set(Integer newHeight) {
        assertIsEnabled();
        super.set(newHeight);
    }

    @Override
    public Integer addObserver(Callback<Integer> obs) {
        assertIsEnabled();
        return super.addObserver(obs);
    }

    private void assertIsEnabled() {
        assert ChromeFeatureList.sDynamicTopChrome.isEnabled()
                : "TabStripHeightSupplier set or observed without DTC enabled.";
    }
}
