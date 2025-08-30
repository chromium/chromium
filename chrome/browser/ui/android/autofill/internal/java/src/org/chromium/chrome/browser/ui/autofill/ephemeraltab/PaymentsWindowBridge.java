// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill.ephemeraltab;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

/** Bridge for the Emphemeral Tab bottom sheet. */
@JNINamespace("autofill::payments")
@NullMarked
class PaymentsWindowBridge {
    private long mNativeAutofillPaymentsWindowBridge;
    private PaymentsWindowCoordinator mPaymentsWindowCoordinator;

    @CalledByNative
    PaymentsWindowBridge(long nativeAutofillPaymentsWindowBridge, WebContents webContents) {
        mNativeAutofillPaymentsWindowBridge = nativeAutofillPaymentsWindowBridge;
        mPaymentsWindowCoordinator = new PaymentsWindowCoordinator(webContents, this);
    }

    @CalledByNative
    public void openEphemeralTab(GURL url, String title) {
        mPaymentsWindowCoordinator.openEphemeralTab(url, title);
    }

    @CalledByNative
    public void closeEphemeralTab() {
        mPaymentsWindowCoordinator.closeEphemeralTab();
    }

    PaymentsWindowCoordinator getPaymentsWindowCoordinatorForTesting() {
        return mPaymentsWindowCoordinator;
    }

    void setPaymentsWindowCoordinatorForTesting(
            PaymentsWindowCoordinator paymentsWindowCoordinator) {
        mPaymentsWindowCoordinator = paymentsWindowCoordinator;
    }

    /**
     * Called when the current navigation to a page has finished.
     *
     * @param clickedUrl The URL that the user initiated the navigation to.
     */
    void onNavigationFinished(GURL clickedUrl) {
        if (mNativeAutofillPaymentsWindowBridge == 0) return;
        PaymentsWindowBridgeJni.get()
                .onNavigationFinished(mNativeAutofillPaymentsWindowBridge, clickedUrl);
    }

    /**
     * Called when {@code WebContents} is being destroyed.
     *
     * <p>After this call, clients should assume that {@code WebContents} will be imminently
     * destroyed and the C++ counterpart deleted.
     */
    void onWebContentsDestroyed() {
        if (mNativeAutofillPaymentsWindowBridge == 0) return;
        PaymentsWindowBridgeJni.get().onWebContentsDestroyed(mNativeAutofillPaymentsWindowBridge);
        mNativeAutofillPaymentsWindowBridge = 0;
    }

    @NativeMethods
    interface Natives {
        void onNavigationFinished(long nativeAutofillPaymentsWindowBridge, GURL clickedUrl);

        void onWebContentsDestroyed(long nativeAutofillPaymentsWindowBridge);
    }
}
