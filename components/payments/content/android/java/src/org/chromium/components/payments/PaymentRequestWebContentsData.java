// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.UserData;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContents.UserDataFactory;
import org.chromium.content_public.browser.WebContentsObserver;

/**
 * Tracks information related to PaymentRequest calls that needs to outlive individual
 * PaymentRequest objects, e.g. whether there has been a call to PaymentRequest.show() without a
 * user activation, which is recorded and tracked by the native PaymentRequestWebContentsManager.
 */
@JNINamespace("payments::android")
@NullMarked
public class PaymentRequestWebContentsData extends WebContentsObserver implements UserData {
    private static @Nullable PaymentRequestWebContentsData sInstanceForTesting;

    private static final class UserDataFactoryLazyHolder {
        private static final UserDataFactory<PaymentRequestWebContentsData> INSTANCE =
                PaymentRequestWebContentsData::new;
    }

    /** Set the instance directly for testing this class. */
    public static void setInstanceForTesting(PaymentRequestWebContentsData instance) {
        sInstanceForTesting = instance;
    }

    /**
     * Returns or creates the the WebContents owned instance of PaymentRequestWebContentsData.
     *
     * @param webContents The web contents of the current PaymentRequest.
     * @return the PaymentRequestWebContentsData instance.
     */
    public static @Nullable PaymentRequestWebContentsData from(WebContents webContents) {
        return sInstanceForTesting != null
                ? sInstanceForTesting
                : webContents.getOrSetUserData(
                        PaymentRequestWebContentsData.class, UserDataFactoryLazyHolder.INSTANCE);
    }

    /** Constructor used for creating a test instance. */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public PaymentRequestWebContentsData(WebContents webContents) {
        super(webContents);
    }

    /**
     * @return Whether there has been an activationless PaymentRequest.show() for this WebContents.
     */
    public boolean hadActivationlessShow() {
        WebContents webContents = getWebContents();
        if (webContents == null || webContents.isDestroyed()) return false;
        return PaymentRequestWebContentsDataJni.get().hadActivationlessShow(webContents);
    }

    /**
     * Called when there has been an activationless PaymentRequest.show(), which is recorded and
     * tracked on the native side in PaymentRequestWebContentsManager.
     */
    public void recordActivationlessShow() {
        WebContents webContents = getWebContents();
        if (webContents == null || webContents.isDestroyed()) return;
        PaymentRequestWebContentsDataJni.get().recordActivationlessShow(webContents);
    }

    public @SPCTransactionMode int getSPCTransactionMode() {
        WebContents webContents = getWebContents();
        if (webContents == null || webContents.isDestroyed()) {
            return SPCTransactionMode.NONE;
        }
        return PaymentRequestWebContentsDataJni.get().getSPCTransactionMode(webContents);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    @NativeMethods
    public interface Natives {
        boolean hadActivationlessShow(WebContents webContents);

        void recordActivationlessShow(WebContents webContents);

        int getSPCTransactionMode(WebContents webContents);
    }
}
