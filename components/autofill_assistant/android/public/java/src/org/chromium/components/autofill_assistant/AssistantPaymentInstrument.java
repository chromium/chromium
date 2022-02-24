// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant;

import androidx.annotation.Nullable;

/**
 * A payment instrument, wrapping a {@link AssistantAutofillCreditCard} and an {@link
 * AssistantAutofillProfile}.
 */
public class AssistantPaymentInstrument {
    private final AssistantAutofillCreditCard mCreditCard;
    private final @Nullable AssistantAutofillProfile mBillingAddress;

    public AssistantPaymentInstrument(AssistantAutofillCreditCard creditCard,
            @Nullable AssistantAutofillProfile billingAddress) {
        mCreditCard = creditCard;
        mBillingAddress = billingAddress;
    }

    public AssistantAutofillCreditCard getCreditCard() {
        return mCreditCard;
    }

    @Nullable
    public AssistantAutofillProfile getBillingAddress() {
        return mBillingAddress;
    }
}
