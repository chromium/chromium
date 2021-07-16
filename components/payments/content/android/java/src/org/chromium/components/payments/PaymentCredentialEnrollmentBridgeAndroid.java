// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import android.graphics.Bitmap;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.payments.secure_payment_confirmation.PaymentCredentialEnrollmentController;
import org.chromium.content_public.browser.WebContents;

/**
 * The Android-specific part of the payment-credential-enrollment view controller. The
 * cross-platform part of the controller is PaymentCredentialEnrollmentController. For new
 * controller logic, please try to make it cross-platform and share with the Desktop side.
 */
public class PaymentCredentialEnrollmentBridgeAndroid {
    private static PaymentCredentialEnrollmentController sSPCEnrollmentController;

    @CalledByNative
    private static void showDialog(WebContents webContents, String instrumentName,
            boolean isIncognito, Bitmap icon, final long responseCallback) {
        assert sSPCEnrollmentController == null;
        sSPCEnrollmentController = PaymentCredentialEnrollmentController.create(webContents);
        if (!sSPCEnrollmentController.show((accepted) -> {
                closeDialog();
                PaymentCredentialEnrollmentBridgeAndroidJni.get().onResponse(
                        responseCallback, accepted);
            }, instrumentName, icon, isIncognito)) {
            closeDialog();
        }
    }

    // TODO(crbug.com/1227490): Remove this method from the native interface as it's not being used.
    @CalledByNative
    private static void showProcessingSpinner() {
        // SPC Enrollment UI will be covered by FIDO UI, not needed for Android implementation.
    }

    @CalledByNative
    private static void closeDialog() {
        if (sSPCEnrollmentController != null) {
            sSPCEnrollmentController.hide();
            sSPCEnrollmentController = null;
        }
    }

    @NativeMethods
    interface Natives {
        void onResponse(long callback, boolean accepted);
    }
}
