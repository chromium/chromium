// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.fast_checkout;

import org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.ScreenType;

/** Provides read-only information about the state of the Fast Checkout bottomsheet. */
public interface FastCheckoutSheetState {
    /** Returns the current screen type of the bottomsheet. */
    @ScreenType
    int getCurrentScreen();

    /** Returns the number of Autofill profiles that would currently be displayed to the user. */
    int getNumOfAutofillProfiles();

    /** Returns the number of credit cards that would currently be displayed to the user. */
    int getNumOfCreditCards();

    /** Returns the height of the bottomsheet's container, i.e. the screen. */
    int getContainerHeight();
}
