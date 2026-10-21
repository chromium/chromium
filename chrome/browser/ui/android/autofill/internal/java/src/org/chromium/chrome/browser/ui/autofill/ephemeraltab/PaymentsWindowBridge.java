// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill.ephemeraltab;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniPtr;
import org.jni_zero.JniRawPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

/** Bridge for the Emphemeral Tab bottom sheet. */
@JNINamespace("autofill::payments")
@NullMarked
class PaymentsWindowBridge {
    @JniType("::autofill::payments::PaymentsWindowBridge")
    interface NativePaymentsWindowBridge extends JniTypeToken {}

    private @Nullable JniRawPtr<NativePaymentsWindowBridge> mNativePaymentsWindowBridge;
    private PaymentsWindowCoordinator mPaymentsWindowCoordinator;

    @CalledByNative
    PaymentsWindowBridge(JniRawPtr<NativePaymentsWindowBridge> nativePaymentsWindowBridge) {
        mNativePaymentsWindowBridge = nativePaymentsWindowBridge;
        mPaymentsWindowCoordinator = new PaymentsWindowCoordinator(this);
    }

    @CalledByNative
    public void openEphemeralTab(
            @JniType("GURL") GURL url,
            @JniType("std::u16string") String title,
            @JniType("content::WebContents*") WebContents merchantWebContents) {
        mPaymentsWindowCoordinator.openEphemeralTab(url, title, merchantWebContents);
    }

    @CalledByNative
    public void closeEphemeralTab() {
        mPaymentsWindowCoordinator.closeEphemeralTab();
    }

    @CalledByNative
    public void onNativeDestroyed() {
        if (mNativePaymentsWindowBridge != null) {
            mNativePaymentsWindowBridge.release();
            mNativePaymentsWindowBridge = null;
        }
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
        if (mNativePaymentsWindowBridge == null) return;
        PaymentsWindowBridgeJni.get().onNavigationFinished(mNativePaymentsWindowBridge, clickedUrl);
    }

    /**
     * Called when the observation of the WebContents has started.
     *
     * @param webContents The WebContents that is being observed.
     */
    public void onWebContentsObservationStarted(WebContents webContents) {
        if (mNativePaymentsWindowBridge == null) return;
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
        if (mNativePaymentsWindowBridge == null) return;
        PaymentsWindowBridgeJni.get().onWebContentsDestroyed(mNativePaymentsWindowBridge);
    }

    void onUserDeniedTabOpening() {
        if (mNativePaymentsWindowBridge == null) return;
        PaymentsWindowBridgeJni.get().onUserDeniedTabOpening(mNativePaymentsWindowBridge);
    }

    @NativeMethods
    interface Natives {
        void onNavigationFinished(
                JniPtr<NativePaymentsWindowBridge> self, @JniType("GURL") GURL clickedUrl);

        void onWebContentsObservationStarted(
                JniPtr<NativePaymentsWindowBridge> self,
                @JniType("content::WebContents*") WebContents webContents);

        void onWebContentsDestroyed(JniPtr<NativePaymentsWindowBridge> self);

        void onUserDeniedTabOpening(JniPtr<NativePaymentsWindowBridge> self);
    }
}
