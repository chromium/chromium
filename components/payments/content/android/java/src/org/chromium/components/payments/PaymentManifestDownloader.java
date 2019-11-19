// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.content_public.browser.WebContents;

import java.net.URI;

/**
 * See comment in:
 * components/payments/core/payment_manifest_downloader.h
 */
@JNINamespace("payments")
public class PaymentManifestDownloader {
    /** Interface for the callback to invoke when finished downloading. */
    public interface ManifestDownloadCallback {
        /**
         * Called on successful download of a payment method manifest.
         *
         * @param content The successfully downloaded payment method manifest.
         */
        @CalledByNative("ManifestDownloadCallback")
        void onPaymentMethodManifestDownloadSuccess(String content);

        /**
         * Called on successful download of a web app manifest.
         *
         * @param content The successfully downloaded web app manifest.
         */
        @CalledByNative("ManifestDownloadCallback")
        void onWebAppManifestDownloadSuccess(String content);

        /**
         * Called on failed download.
         *
         * @param errorMessage The error message, which could be empty or null.
         */
        @CalledByNative("ManifestDownloadCallback")
        void onManifestDownloadFailure(String errorMessage);
    }

    private long mNativeObject;

    /**
     * Initializes the native downloader.
     *
     * @param webContents The web contents to use as the context for the downloads. If this goes
     *                    away, pending downloads are cancelled.
     */
    public void initialize(WebContents webContents) {
        ThreadUtils.assertOnUiThread();
        assert mNativeObject == 0;
        mNativeObject = PaymentManifestDownloaderJni.get().init(webContents);
    }

    /** @return Whether the native downloader is initialized. */
    public boolean isInitialized() {
        ThreadUtils.assertOnUiThread();
        return mNativeObject != 0;
    }

    /**
     * Downloads the payment method manifest file asynchronously.
     *
     * @param methodName The payment method name that is a URI with HTTPS scheme.
     * @param callback   The callback to invoke when finished downloading.
     */
    public void downloadPaymentMethodManifest(URI methodName, ManifestDownloadCallback callback) {
        ThreadUtils.assertOnUiThread();
        assert mNativeObject != 0;
        PaymentManifestDownloaderJni.get().downloadPaymentMethodManifest(
                mNativeObject, PaymentManifestDownloader.this, methodName, callback);
    }

    /**
     * Downloads the web app manifest file asynchronously.
     *
     * @param webAppManifestUri The web app manifest URI with HTTPS scheme.
     * @param callback          The callback to invoke when finished downloading.
     */
    public void downloadWebAppManifest(URI webAppManifestUri, ManifestDownloadCallback callback) {
        ThreadUtils.assertOnUiThread();
        assert mNativeObject != 0;
        PaymentManifestDownloaderJni.get().downloadWebAppManifest(
                mNativeObject, PaymentManifestDownloader.this, webAppManifestUri, callback);
    }

    /** Destroys the native downloader. */
    public void destroy() {
        ThreadUtils.assertOnUiThread();
        assert mNativeObject != 0;
        PaymentManifestDownloaderJni.get().destroy(mNativeObject, PaymentManifestDownloader.this);
        mNativeObject = 0;
    }

    @CalledByNative
    private static String getUriString(URI methodName) {
        return methodName.toString();
    }

    @NativeMethods
    interface Natives {
        long init(WebContents webContents);
        void downloadPaymentMethodManifest(long nativePaymentManifestDownloaderAndroid,
                PaymentManifestDownloader caller, URI methodName,
                ManifestDownloadCallback callback);
        void downloadWebAppManifest(long nativePaymentManifestDownloaderAndroid,
                PaymentManifestDownloader caller, URI webAppManifestUri,
                ManifestDownloadCallback callback);
        void destroy(long nativePaymentManifestDownloaderAndroid, PaymentManifestDownloader caller);
    }
}
