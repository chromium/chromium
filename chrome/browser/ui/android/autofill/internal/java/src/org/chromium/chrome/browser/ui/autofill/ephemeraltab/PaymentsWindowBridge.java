// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill.ephemeraltab;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.build.annotations.NullMarked;
import org.chromium.content_public.browser.WebContents;

/** Bridge for the Emphemeral Tab bottom sheet. */
@JNINamespace("autofill")
@NullMarked
class PaymentsWindowBridge {
    private PaymentsWindowCoordinator mPaymentsWindowCoordinator;

    @CalledByNative
    PaymentsWindowBridge(WebContents webContents) {
        mPaymentsWindowCoordinator = new PaymentsWindowCoordinator(webContents);
    }

    @CalledByNative
    public void openEphemeralTab() {
        mPaymentsWindowCoordinator.openEphemeralTab();
    }

    PaymentsWindowCoordinator getPaymentsWindowCoordinatorForTesting() {
        return mPaymentsWindowCoordinator;
    }

    void setPaymentsWindowCoordinatorForTesting(
            PaymentsWindowCoordinator paymentsWindowCoordinator) {
        mPaymentsWindowCoordinator = paymentsWindowCoordinator;
    }
}
