// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.content_public.browser.WebContents;

/** The navigation throttle of the payment handler pages. */
@JNINamespace("payments::android")
public class PaymentHandlerNavigationThrottle {
    /**
     * Marks the given WebContents as a payment handler WebContents. This will allow the callers of
     * payment_handler_navigation_throttle to identify the payment handler WebContents given its
     * NavigationHandler.
     * @param webContents The payment handler WebContents. Null or destroyed one will be ignored.
     */
    public static void markPaymentHandlerWebContents(WebContents webContents) {
        if (webContents == null || webContents.isDestroyed()) return;
        PaymentHandlerNavigationThrottleJni.get().markPaymentHandlerWebContents(webContents);
    }

    @NativeMethods
    /* package */ interface Natives {
        void markPaymentHandlerWebContents(WebContents webContents);
    }
}
