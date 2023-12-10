// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.fast_checkout.detail_screen;

import org.chromium.chrome.browser.ui.fast_checkout.data.FastCheckoutCreditCard;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;

/** Model for an {@link FastCheckoutCreditCard} entry in the credit card screen sheet. */
public class CreditCardItemProperties {
    /** The credit card represented by this entry. */
    public static final ReadableObjectPropertyKey<FastCheckoutCreditCard> CREDIT_CARD =
            new ReadableObjectPropertyKey<>("credit_card");

    /**
     * An indicator of whether this credit card is the currently selected one. This key
     * is kept in sync by the {@link FastCheckoutMediator};
     */
    public static final WritableBooleanPropertyKey IS_SELECTED =
            new WritableBooleanPropertyKey("is_selected");

    /** The function to run when this credit card is selected by the user. */
    public static final ReadableObjectPropertyKey<Runnable> ON_CLICK_LISTENER =
            new ReadableObjectPropertyKey<>("on_click_listener");

    /** Creates a model for a {@link FastCheckoutCreditCard}. */
    public static PropertyModel create(
            FastCheckoutCreditCard creditCard, boolean isSelected, Runnable onClickListener) {
        return new PropertyModel.Builder(ALL_KEYS)
                .with(CREDIT_CARD, creditCard)
                .with(IS_SELECTED, isSelected)
                .with(ON_CLICK_LISTENER, onClickListener)
                .build();
    }

    public static final PropertyKey[] ALL_KEYS = {CREDIT_CARD, IS_SELECTED, ON_CLICK_LISTENER};
}
