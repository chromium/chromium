// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.util;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.cc.input.BrowserControlsState;

/** A delegate to determine visibility of the browser controls. */
public class BrowserControlsVisibilityDelegate extends ObservableSupplierImpl<Integer> {
    /**
     * Constructs a delegate that controls the visibility of the browser controls.
     * @param initialValue The initial browser state visibility.
     */
    public BrowserControlsVisibilityDelegate(@BrowserControlsState int initialValue) {
        set(initialValue);
    }

    @Override
    public void set(@BrowserControlsState Integer value) {
        assert value != null;
        super.set(value);
    }
}
