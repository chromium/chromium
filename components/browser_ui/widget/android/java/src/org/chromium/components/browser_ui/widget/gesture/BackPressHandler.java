// Copyright 2022 The Chromium Authors
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
    // When adding a new identifier, make corresponding changes in the
    // - tools/metrics/histograms/enums.xml: <enum name="BackPressConsumer">
    // - chrome/browser/back_press/android/.../BackPressManager.java: sMetricsMap
    @IntDef({Type.TEXT_BUBBLE, Type.VR_DELEGATE, Type.AR_DELEGATE, Type.SCENE_OVERLAY,
            Type.START_SURFACE, Type.SELECTION_POPUP, Type.MANUAL_FILLING, Type.TAB_MODAL_HANDLER,
            Type.FULLSCREEN, Type.TAB_SWITCHER, Type.CLOSE_WATCHER, Type.FIND_TOOLBAR,
            Type.LOCATION_BAR, Type.TAB_HISTORY, Type.TAB_RETURN_TO_CHROME_START_SURFACE,
            Type.BOTTOM_SHEET, Type.SHOW_READING_LIST, Type.MINIMIZE_APP_AND_CLOSE_TAB})
    @Retention(RetentionPolicy.SOURCE)
    @interface Type {
        int TEXT_BUBBLE = 0;
        int VR_DELEGATE = 1;
        int AR_DELEGATE = 2;
        int SCENE_OVERLAY = 3;
        int START_SURFACE = 4;
        int SELECTION_POPUP = 5;
        int MANUAL_FILLING = 6;
        int FULLSCREEN = 7;
        int BOTTOM_SHEET = 8;
        int LOCATION_BAR = 9;
        int TAB_MODAL_HANDLER = 10;
        int TAB_SWITCHER = 11;
        int CLOSE_WATCHER = 12;
        int FIND_TOOLBAR = 13;
        int TAB_HISTORY = 14;
        int TAB_RETURN_TO_CHROME_START_SURFACE = 15;
        int SHOW_READING_LIST = 16;
        int MINIMIZE_APP_AND_CLOSE_TAB = 17;
        int NUM_TYPES = MINIMIZE_APP_AND_CLOSE_TAB + 1;
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
