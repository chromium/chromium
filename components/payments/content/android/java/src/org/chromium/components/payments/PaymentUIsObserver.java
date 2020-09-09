// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

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
}
