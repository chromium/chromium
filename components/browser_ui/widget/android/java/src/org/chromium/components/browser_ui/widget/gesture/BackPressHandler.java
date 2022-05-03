// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.gesture;

import androidx.annotation.IntDef;

import org.chromium.base.supplier.ObservableSupplier;

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
    @IntDef({Type.TEXT_BUBBLE, Type.TEST, Type.AR_DELEGATE})
    @Retention(RetentionPolicy.SOURCE)
    @interface Type {
        int TEXT_BUBBLE = 0;
        int AR_DELEGATE = 1;
        // Add new type here and then increment the value of TEST by one.
        int TEST = 2; // This is for test only. Do not use it in production.
        int NUM_TYPES = TEST + 1;
    }

    void handleBackPress();

    /**
     * A {@link ObservableSupplier<Boolean>} which notifies of whether the implementer wants to
     * intercept the back gesture.
     * @return True if the implementer wants to intercept the back gesture.
     */
    ObservableSupplier<Boolean> getHandleBackPressChangedSupplier();
}
