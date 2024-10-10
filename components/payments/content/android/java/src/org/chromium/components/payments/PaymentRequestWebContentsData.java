// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.UserData;
import org.chromium.content.browser.webcontents.WebContentsImpl;
import org.chromium.content.browser.webcontents.WebContentsImpl.UserDataFactory;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;

/**
 * Tracks information related to PaymentRequest calls that needs to outlive individual
 * PaymentRequest objects, e.g. whether there has been a call to PaymentRequest.show() without a
 * user activation, which is recorded and tracked by the native PaymentRequestWebContentsManager.
 */
@JNINamespace("payments::android")
public class PaymentRequestWebContentsData extends WebContentsObserver implements UserData {
    private static PaymentRequestWebContentsData sInstanceForTesting;

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
    public static PaymentRequestWebContentsData from(WebContents webContents) {
        return sInstanceForTesting != null
                ? sInstanceForTesting
                : ((WebContentsImpl) webContents)
                        .getOrSetUserData(
                                PaymentRequestWebContentsData.class,
                                UserDataFactoryLazyHolder.INSTANCE);
    }

    /** Constructor used for creating a test instance. */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public PaymentRequestWebContentsData(WebContents webContents) {
        super(webContents);
    }

    /** @return Whether there has been an activationless PaymentRequest.show() for this WebContents. */
    public boolean hadActivationlessShow() {
        return PaymentRequestWebContentsDataJni.get().hadActivationlessShow(mWebContents.get());
    }

    /**
     * Called when there has been an activationless PaymentRequest.show(), which is recorded and
     * tracked on the native side in PaymentRequestWebContentsManager.
     */
    public void recordActivationlessShow() {
        PaymentRequestWebContentsDataJni.get().recordActivationlessShow(mWebContents.get());
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    @NativeMethods
    public interface Natives {
        boolean hadActivationlessShow(WebContents webContents);

        void recordActivationlessShow(WebContents webContents);
    }
}
