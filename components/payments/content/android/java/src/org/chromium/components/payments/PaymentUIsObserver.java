// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import org.chromium.payments.mojom.PaymentAddress;

/** Interface for observing payment UIs. */
public interface PaymentUIsObserver {
    /** Called when favicon not available for payment request UI. */
    void onPaymentRequestUIFaviconNotAvailable();

    /**
     * Called when the user is leaving the current tab (e.g., tab switched or tab overview mode is
     * shown), upon which the PaymentRequest service should be closed.
     * @param reason The reason of leaving the current tab, to be used as debug message for the
     *         developers.
     */
    void onLeavingCurrentTab(String reason);

    /**
     * Called when the user's selected shipping option has changed.
     * @param optionId The option id of the selected shipping option.
     */
    void onShippingOptionChange(String optionId);

    /**
     * Called when the shipping address has changed by the user.
     * @param address The changed shipping address.
     */
    void onShippingAddressChange(PaymentAddress address);

    /**
     * Called when the Payment UI service quits with an error. The observer should stop referencing
     * the Payment UI service.
     * @param error The diagnostic message that's exposed to developers.
     */
    void onUiServiceError(String error);
}
