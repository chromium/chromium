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
    private final long mNativePaymentsWindowBridge;
    private PaymentsWindowCoordinator mPaymentsWindowCoordinator;

    @CalledByNative
    PaymentsWindowBridge(long nativePaymentsWindowBridge) {
        mNativePaymentsWindowBridge = nativePaymentsWindowBridge;
        mPaymentsWindowCoordinator = new PaymentsWindowCoordinator(this);
    }

    @CalledByNative
    public void openEphemeralTab(GURL url, String title, WebContents merchantWebContents) {
        mPaymentsWindowCoordinator.openEphemeralTab(url, title, merchantWebContents);
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
        PaymentsWindowBridgeJni.get().onNavigationFinished(mNativePaymentsWindowBridge, clickedUrl);
    }

    /**
     * Called when the observation of the WebContents has started.
     *
     * @param webContents The WebContents that is being observed.
     */
    public void onWebContentsObservationStarted(WebContents webContents) {
        PaymentsWindowBridgeJni.get()
                .onWebContentsObservationStarted(mNativePaymentsWindowBridge, webContents);
    }

    /**
     * Called when the observed {@code WebContents} is being destroyed.
     *
     * <p>After this call, clients should assume that the observed {@code WebContents} will be
     * imminently destroyed.
     */
    void onWebContentsDestroyed() {
        PaymentsWindowBridgeJni.get().onWebContentsDestroyed(mNativePaymentsWindowBridge);
    }

    @NativeMethods
    interface Natives {
        void onNavigationFinished(long nativePaymentsWindowBridge, GURL clickedUrl);

        void onWebContentsObservationStarted(
                long nativePaymentsWindowBridge, WebContents webContents);

        void onWebContentsDestroyed(long nativePaymentsWindowBridge);
    }
}
