// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import android.graphics.Bitmap;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.content_public.browser.WebContents;

/**
 * The Android-specific part of the payment-credential-enrollment view controller. The
 * cross-platform part of the controller is PaymentCredentialEnrollmentController. For new
 * controller logic, please try to make it cross-platform and share with the Desktop side.
 */
public class PaymentCredentialEnrollmentBridgeAndroid {
    @CalledByNative
    private static void showDialog(WebContents webContents, String instrumentName,
            boolean isIncognito, Bitmap icon, final long responseCallback) {
        // TODO(crbug.com/1204564): Before the UI is implemented, the enrollment request is
        // automatically accepted.
        PaymentCredentialEnrollmentBridgeAndroidJni.get().onResponse(responseCallback, true);
    }

    @CalledByNative
    private static void showProcessingSpinner() {
        // TODO(crbug.com/1204564): Implement it.
    }

    @CalledByNative
    private static void closeDialog() {
        // TODO(crbug.com/1204564): Implement it.
    }

    @NativeMethods
    interface Natives {
        void onResponse(long callback, boolean accepted);
    }
}
