// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.util;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.cc.input.BrowserControlsState;

/** A delegate to determine visibility of the browser controls. */
@NullMarked
public class BrowserControlsVisibilityDelegate
        extends ObservableSupplierImpl<@BrowserControlsState Integer> {
    /**
     * Constructs a delegate that controls the visibility of the browser controls.
     *
     * @param initialValue The initial browser state visibility.
     */
    public BrowserControlsVisibilityDelegate(@BrowserControlsState int initialValue) {
        super.set(initialValue);
    }
}
