// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.fast_checkout;

import androidx.annotation.IntDef;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * State for the Fast Checkout UI.
 */
public class FastCheckoutModel {
    /**
     * The different screens that can be shown on the sheet.
     */
    @IntDef({ScreenType.HOME_SCREEN, ScreenType.AUTOFILL_PROFILES_SCREEN,
            ScreenType.CREDIT_CARDS_SCREEN})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ScreenType {
        int HOME_SCREEN = 0;
        int AUTOFILL_PROFILES_SCREEN = 1;
        int CREDIT_CARDS_SCREEN = 2;
    }

    /** Property that indicates the bottom sheet visibility. */
    public static final PropertyModel.WritableBooleanPropertyKey VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();

    /**
     * Property that indicates which screen (i.e ScreenType) is currently displayed on the bottom
     * sheet.
     */
    public static final PropertyModel.WritableIntPropertyKey CURRENT_SCREEN =
            new PropertyModel.WritableIntPropertyKey();

    static PropertyModel createDefaultModel() {
        return new PropertyModel.Builder(ALL_KEYS)
                .with(VISIBLE, false)
                .with(CURRENT_SCREEN, ScreenType.HOME_SCREEN)
                .build();
    }

    /** All keys used for the fast checkout bottom sheet. */
    static final PropertyKey[] ALL_KEYS = new PropertyKey[] {CURRENT_SCREEN, VISIBLE};
}
