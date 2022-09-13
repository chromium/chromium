// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant;

import androidx.annotation.Nullable;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Model wrapper for a data item to contain errors.
 *
 * @param <T> The type that an instance of this class is created for, such as
 *            {@link AssistantAutofillProfile}, {@link AssistantPaymentInstrument}, etc.
 */
public abstract class AssistantOptionModel<T> {
    public T mOption;
    public List<String> mErrors;

    public AssistantOptionModel(T option, List<String> errors) {
        this.mOption = option;
        this.mErrors = errors;
    }

    public AssistantOptionModel(T option) {
        this(option, new ArrayList<>());
    }

    public boolean isComplete() {
        return mErrors.isEmpty();
    }

    public abstract boolean canEdit();

    /** Model wrapper for an {@link AssistantAutofillProfile}. */
    public static class ContactModel extends AssistantOptionModel<AssistantAutofillProfile> {
        private final boolean mCanEdit;

        public ContactModel(
                AssistantAutofillProfile contact, List<String> errors, boolean canEdit) {
            super(contact, errors);
            mCanEdit = canEdit;
        }

        public ContactModel(AssistantAutofillProfile contact) {
            super(contact);
            mCanEdit = true;
        }

        @Override
        public boolean canEdit() {
            return mCanEdit;
        }
    }

    /** Model wrapper for an {@link AssistantAutofillProfile}. */
    public static class AddressModel extends AssistantOptionModel<AssistantAutofillProfile> {
        private final String mFullDescription;
        private final String mSummaryDescription;
        @Nullable
        private final byte[] mEditToken;

        public AddressModel(AssistantAutofillProfile address, String fullDescription,
                String summaryDescription, List<String> errors, @Nullable byte[] editToken) {
            super(address, errors);
            mFullDescription = fullDescription;
            mSummaryDescription = summaryDescription;
            mEditToken = editToken;
        }

        public AddressModel(AssistantAutofillProfile address, String fullDescription,
                String summaryDescription) {
            this(address, fullDescription, summaryDescription,
                    /* errors= */ Collections.emptyList(),
                    /* editToken= */ null);
        }

        public String getFullDescription() {
            return mFullDescription;
        }

        public String getSummaryDescription() {
            return mSummaryDescription;
        }

        @Override
        public boolean canEdit() {
            return mEditToken == null || mEditToken.length > 0;
        }

        @Nullable
        byte[] getEditToken() {
            return mEditToken;
        }
    }

    /** Model wrapper for an {@code AssistantPaymentInstrument}. */
    public static class PaymentInstrumentModel
            extends AssistantOptionModel<AssistantPaymentInstrument> {
        @Nullable
        private final byte[] mEditToken;

        public PaymentInstrumentModel(AssistantPaymentInstrument paymentInstrument,
                List<String> errors, @Nullable byte[] editToken) {
            super(paymentInstrument, errors);
            mEditToken = editToken;
        }

        public PaymentInstrumentModel(AssistantPaymentInstrument paymentInstrument) {
            this(paymentInstrument, /* errors= */ Collections.emptyList(), /* editToken= */ null);
        }

        @Override
        public boolean canEdit() {
            return mEditToken == null || mEditToken.length > 0;
        }

        @Nullable
        byte[] getEditToken() {
            return mEditToken;
        }
    }
}
