// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.gesture;

import androidx.activity.BackEventCompat;
import androidx.annotation.IntDef;
import androidx.annotation.NonNull;

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
    @IntDef({
        Type.TEXT_BUBBLE,
        Type.VR_DELEGATE,
        Type.XR_DELEGATE,
        Type.SCENE_OVERLAY,
        Type.START_SURFACE,
        Type.SELECTION_POPUP,
        Type.MANUAL_FILLING,
        Type.TAB_MODAL_HANDLER,
        Type.FULLSCREEN,
        Type.HUB,
        Type.TAB_SWITCHER,
        Type.CLOSE_WATCHER,
        Type.FIND_TOOLBAR,
        Type.LOCATION_BAR,
        Type.BOTTOM_CONTROLS,
        Type.TAB_HISTORY,
        Type.TAB_RETURN_TO_CHROME_START_SURFACE,
        Type.BOTTOM_SHEET,
        Type.PAGE_INSIGHTS_BOTTOM_SHEET,
        Type.SHOW_READING_LIST,
        Type.MINIMIZE_APP_AND_CLOSE_TAB
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface Type {
        int TEXT_BUBBLE = 0;
        int VR_DELEGATE = 1;
        int XR_DELEGATE = 2;
        int SCENE_OVERLAY = 3;
        int BOTTOM_SHEET = 4;
        // TODO(b/307046796): Remove this once we have found better way to integrate with back
        // handling logic.
        int PAGE_INSIGHTS_BOTTOM_SHEET = 5;
        int START_SURFACE = 6;
        int HUB = 7;
        int TAB_SWITCHER = 8;
        // Fullscreen must be before selection popup. crbug.com/1454817.
        int FULLSCREEN = 9;
        int SELECTION_POPUP = 10;
        int MANUAL_FILLING = 11;
        int LOCATION_BAR = 12;
        int TAB_MODAL_HANDLER = 13;
        int CLOSE_WATCHER = 14;
        int FIND_TOOLBAR = 15;
        int BOTTOM_CONTROLS = 16;
        int TAB_HISTORY = 17;
        int TAB_RETURN_TO_CHROME_START_SURFACE = 18;
        int SHOW_READING_LIST = 19;
        int MINIMIZE_APP_AND_CLOSE_TAB = 20;
        int NUM_TYPES = MINIMIZE_APP_AND_CLOSE_TAB + 1;
    }

    /** Result of back press handling. */
    @IntDef({BackPressResult.SUCCESS, BackPressResult.FAILURE, BackPressResult.UNKNOWN})
    @Retention(RetentionPolicy.SOURCE)
    @interface BackPressResult {
        // Successfully intercept the back press and does something to handle the back press,
        // e.g. making a UI change.
        int SUCCESS = 0;
        // Failure usually means intercepting a back press when the handler wasn't supposed to, such
        // as #handleBackPress is called, but nothing is committed by the client. This is an
        // indication something isn't working properly.
        int FAILURE = 1;
        // Do not use unless it is not possible to verify if the back press was correctly handled.
        int UNKNOWN = 2;
        int NUM_TYPES = UNKNOWN + 1;
    }

    /**
     * The modern way to handle back press. This method is only called when {@link
     * #getHandleBackPressChangedSupplier()} returns true; i.e. the back press has been intercepted
     * by chrome and the client must do something to consume the back press. So ideally, this is
     * **always** expected to return {@link BackPressResult#SUCCESS}.
     * A {@link BackPressResult#FAILURE} means the back press is intercepted but somehow the client
     * does not consume; i.e. makes no change. This is usually because the client didn't update
     * {@link #getHandleBackPressChangedSupplier()} such that this method is called even when the
     * client does not want. A Failure means Chrome is now incorrectly working and should be
     * fixed ASAP.
     * The difference between this and the traditional way {@code boolean #onBackPressed} is that
     * the traditional one gives the client an opportunity to test if the client wants to intercept.
     * If it returns false, the tradition way will simply test other clients.
     * @return Whether the back press has been correctly handled.
     */
    default @BackPressResult int handleBackPress() {
        return BackPressResult.UNKNOWN;
    }

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

    /**
     * API 34+ only. Triggered when a back press press is cancelled. In this case,
     * {@link #handleBackPress()} must not be triggered any more.
     */
    default void handleOnBackCancelled() {}

    /**
     * API 34+ only. Triggered after a back press is started
     * ({@link #handleOnBackStarted(BackEventCompat)}) and before a back press is released
     * (either {@link #handleBackPress()} or {@link #handleOnBackCancelled()})
     */
    default void handleOnBackProgressed(@NonNull BackEventCompat backEvent) {}

    /** API 34+ only. Triggered when a back press event is initialized. */
    default void handleOnBackStarted(@NonNull BackEventCompat backEvent) {}
}
