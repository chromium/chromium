// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.components.autofill_assistant.AssistantOptionModel.AddressModel;
import org.chromium.components.autofill_assistant.AssistantOptionModel.ContactModel;
import org.chromium.components.autofill_assistant.AssistantOptionModel.PaymentInstrumentModel;

/**
 * Generic Editor interface.
 *
 * @param <T> The type of {@link AssistantOptionModel} this editor will operate on.
 */
public interface AssistantEditor<T extends AssistantOptionModel> {
    /**
     * Create or edit an item. If |oldItem| is null, a new item is being created, otherwise the
     * provided item is edited. The |callback| is invoked with the new item after the editor is
     * closed with saving the changes.
     *
     * @param oldItem The item to be edited, can be null in which case a new item is created.
     * @param doneCallback Called after the editor is closed, assuming that the item has been
     *                     successfully edited.
     * @param cancelCallback Called after the editor is closed, assuming that the edit has been
     *                       abandoned.
     */
    void createOrEditItem(
            @Nullable T oldItem, Callback<T> doneCallback, Callback<T> cancelCallback);

    /**
     * Editor for {@link ContactModel}.
     */
    interface AssistantContactEditor extends AssistantEditor<ContactModel> {
        /**
         * Adds information from the contact to the UI of the editor.
         * @param contact The {@link AssistantAutofillProfile} to add information for.
         */
        default void addContactInformationForAutocomplete(AssistantAutofillProfile contact) {}
    }

    /**
     * Editor for {@link AddressModel}.
     */
    interface AssistantAddressEditor extends AssistantEditor<AddressModel> {}

    /**
     * Editor for {@link PaymentInstrumentModel}.
     */
    interface AssistantPaymentInstrumentEditor extends AssistantEditor<PaymentInstrumentModel> {
        /**
         * Adds information from the address to the UI of the editor.
         * @param address The {@link AssistantAutofillProfile} to add information for.
         */
        default void addAddressInformationForAutocomplete(AssistantAutofillProfile address) {}
    }
}
