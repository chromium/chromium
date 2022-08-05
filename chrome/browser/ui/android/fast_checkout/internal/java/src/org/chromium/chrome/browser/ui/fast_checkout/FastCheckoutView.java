// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.fast_checkout;

import android.view.View;

import org.chromium.base.Callback;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/**
 * The {@link BottomSheetContent} for Fast Checkout. TODO(crbug.com/1334642): The view
 * should implement BottomSheetContent.
 */
public class FastCheckoutView {
    private final BottomSheetController mBottomSheetController;
    private final View mContentView;

    /**
     * Constructs a FastCheckoutView which creates, modifies, and shows the bottom sheet.
     */
    FastCheckoutView(View contentView, BottomSheetController bottomSheetController) {
        mBottomSheetController = bottomSheetController;
        mContentView = contentView;
    }

    /**
     * Sets a new listener that reacts to events like item selection or dismissal.
     * @param dismissHandler A {@link Callback<Integer>}.
     */
    void setDismissHandler(Callback<Integer> dismissHandler) {
        // TODO(crbug.com/1334642): Implement.
    }

    /**
     * If set to true, requests to show the bottom sheet. Otherwise, requests to hide the sheet.
     * @param isVisible A boolean describing whether to show or hide the sheet.
     * @return True if the request was successful, false otherwise.
     */
    boolean setVisible(boolean isVisible) {
        // TODO(crbug.com/1334642): Implement.
        return false;
    }

    /**
     * Sets the screen to show on the bottom sheet.
     * @param screenType A {@link ScreenType} specifying the screen to show.
     */
    void updateCurrentScreen(int screenType) {
        // TODO(crbug.com/1334642): Implement.
    }
}
