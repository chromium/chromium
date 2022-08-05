// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.fast_checkout;

import static org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutModel.DISMISS_HANDLER;
import static org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutModel.VISIBLE;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Provides functions that map {@link FastCheckoutModel} changes to the suitable
 * method in {@link FastCheckoutView}.
 */
public class FastCheckoutViewBinder {
    static void bindFastCheckoutView(
            PropertyModel model, FastCheckoutView view, PropertyKey propertyKey) {
        if (FastCheckoutModel.CURRENT_SCREEN == propertyKey) {
            view.updateCurrentScreen(model.get(FastCheckoutModel.CURRENT_SCREEN));
        } else if (FastCheckoutModel.DISMISS_HANDLER == propertyKey) {
            view.setDismissHandler(model.get(DISMISS_HANDLER));
        } else if (FastCheckoutModel.VISIBLE == propertyKey) {
            // Dismiss the sheet if it can't be immediately shown.
            boolean visibilityChangeSuccessful = view.setVisible(model.get(VISIBLE));
            if (!visibilityChangeSuccessful && model.get(VISIBLE)) {
                assert (model.get(DISMISS_HANDLER) != null);
                model.get(DISMISS_HANDLER).onResult(BottomSheetController.StateChangeReason.NONE);
            }
        }
    }
}
