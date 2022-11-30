// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import org.chromium.content_public.browser.WebContents;

/** The interface of PaymentUiService that provides testing methods. */
public interface PaymentUiServiceTestInterface {
    /**
     * Get the WebContents of the Payment Handler; return null if nonexistent.
     *
     * @return The WebContents of the Payment Handler.
     */
    WebContents getPaymentHandlerWebContentsForTest();

    /**
     * Clicks the security icon of the Payment Handler; return false if failed.
     *
     * @return Whether the click is successful.
     */
    boolean clickPaymentHandlerSecurityIconForTest();

    /**
     * Simulates a click on the close button of the Payment Handler; return
     * false if failed.
     *
     * @return Whether the click is successful.
     */
    boolean clickPaymentHandlerCloseButtonForTest();

    /**
     * Closes the payment UI.
     *
     * @return Whether the closing was successful.
     */
    boolean closeDialogForTest();
}
