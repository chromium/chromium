// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.fast_checkout.autofill_profile_screen;

import org.chromium.chrome.browser.ui.fast_checkout.data.FastCheckoutAutofillProfile;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;

/**
 * Model for an {@link FastCheckoutAutofillProfile} entry in the autofill Profile screen sheet.
 */
public class AutofillProfileItemProperties {
    public static final int DEFAULT_ITEM_TYPE = 0;

    /** The profile represented by this entry. */
    public static final ReadableObjectPropertyKey<FastCheckoutAutofillProfile> AUTOFILL_PROFILE =
            new ReadableObjectPropertyKey<>("autofill_profile");

    /**
     * An indicator of whether this profile is the currently selected one. This key is kept
     * in sync by the {@link FastCheckoutMediator};
     */
    public static final WritableBooleanPropertyKey IS_SELECTED =
            new WritableBooleanPropertyKey("is_selected");

    /**
     * The function to run when this profile item is selected by the user.
     */
    public static final ReadableObjectPropertyKey<Runnable> ON_CLICK_LISTENER =
            new ReadableObjectPropertyKey<>("on_click_listener");

    /** Creates a model for a FastCheckoutAutofillProfile. */
    public static PropertyModel create(
            FastCheckoutAutofillProfile profile, boolean isSelected, Runnable onClickListener) {
        return new PropertyModel.Builder(ALL_KEYS)
                .with(AUTOFILL_PROFILE, profile)
                .with(IS_SELECTED, isSelected)
                .with(ON_CLICK_LISTENER, onClickListener)
                .build();
    }

    public static final PropertyKey[] ALL_KEYS = {AUTOFILL_PROFILE, IS_SELECTED, ON_CLICK_LISTENER};
}
