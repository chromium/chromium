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
    @IntDef({Type.TEXT_BUBBLE, Type.VR_DELEGATE, Type.AR_DELEGATE, Type.LAYOUT_MANAGER,
            Type.START_SURFACE_MEDIATOR, Type.SELECTION_POPUP, Type.MANUAL_FILLING,
            Type.TAB_MODAL_HANDLER, Type.FULLSCREEN})
    @Retention(RetentionPolicy.SOURCE)
    @interface Type {
        int TEXT_BUBBLE = 0;
        int VR_DELEGATE = 1;
        int AR_DELEGATE = 2;
        int LAYOUT_MANAGER = 3;
        int START_SURFACE_MEDIATOR = 4;
        int SELECTION_POPUP = 5;
        int MANUAL_FILLING = 6;
        int FULLSCREEN = 7;
        int TAB_MODAL_HANDLER = 8;
        int NUM_TYPES = TAB_MODAL_HANDLER + 1;
    }

    default void handleBackPress() {}

    /**
     * A {@link ObservableSupplier<Boolean>} which notifies of whether the implementer wants to
     * intercept the back gesture.
     * @return An {@link ObservableSupplier<Boolean>} which yields true if the implementer wants to
     *         intercept the back gesture; otherwise, it should yield false to prevent {@link
     *         #handleBackPress()} from being called.
     */
    default ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return new ObservableSupplierImpl<>();
    }
}
