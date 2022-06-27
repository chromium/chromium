// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.fast_checkout;

import android.content.Context;

import org.chromium.chrome.browser.ui.fast_checkout.data.FastCheckoutAutofillProfile;
import org.chromium.chrome.browser.ui.fast_checkout.data.FastCheckoutCreditCard;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

class FastCheckoutCoordinator implements FastCheckoutComponent {
    @Override
    public void initialize(Context context, BottomSheetController sheetController,
            FastCheckoutComponent.Delegate delegate) {
        // TODO(crbug.com/1334642): Implement.
    }

    @Override
    public void showOptions(
            FastCheckoutAutofillProfile[] profiles, FastCheckoutCreditCard[] creditCards) {
        // TODO(crbug.com/1334642): Implement.
    }
}
