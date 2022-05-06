// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.gesture;

import androidx.annotation.IntDef;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * An interface to notify whether the implementer is going to intercept the back press event
 * or not. If the supplier yields false, {@link #handleBackPress()} will never
 * be called; otherwise, when press event is triggered, it will be called, unless any other
 * implementer registered earlier has already consumed the back press event.
 */
public interface BackPressHandler {
    // The smaller the value is, the higher the priority is.
    @IntDef({Type.TEXT_BUBBLE, Type.AR_DELEGATE, Type.LAYOUT_MANAGER, Type.MANUAL_FILLING,
            Type.TAB_MODAL_HANDLER})
    @Retention(RetentionPolicy.SOURCE)
    @interface Type {
        int TEXT_BUBBLE = 0;
        int AR_DELEGATE = 1;
        int LAYOUT_MANAGER = 2;
        int MANUAL_FILLING = 3;
        int TAB_MODAL_HANDLER = 4;
        int NUM_TYPES = TAB_MODAL_HANDLER + 1;
    }

    default void handleBackPress() {}

    /**
     * A {@link ObservableSupplier<Boolean>} which notifies of whether the implementer wants to
     * intercept the back gesture.
     * @return True if the implementer wants to intercept the back gesture.
     */
    default ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return new ObservableSupplierImpl<>();
    }
}
