// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ThreadUtils;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

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
         * @param paymentMethodManifestUrl The URL of the payment method manifest after all
         * redirects and the optional HTTP Link rel=payment-method-manifest header have been
         * followed.
         * @param paymentMethodManifestOrigin The origin of the payment method manifest after all
         * redirects and the optional HTTP Link rel=payment-method-manifest header have been
         * followed.
         * @param content The successfully downloaded payment method manifest.
         */
        @CalledByNative("ManifestDownloadCallback")
        void onPaymentMethodManifestDownloadSuccess(
                GURL paymentMethodManifestUrl, Origin paymentMethodManifestOrigin, String content);

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
    private CSPCheckerBridge mCSPCheckerBridge;

    /**
     * Initializes the native downloader.
     *
     * @param webContents The web contents to use as the context for the downloads. If this goes
     *                    away, pending downloads are cancelled.
     * @param cspChecker The Content-Security-Policy (CSP) checker.
     */
    public void initialize(WebContents webContents, CSPChecker cspChecker) {
        ThreadUtils.assertOnUiThread();
        assert mNativeObject == 0;
        mCSPCheckerBridge = new CSPCheckerBridge(cspChecker);
        mNativeObject =
                PaymentManifestDownloaderJni.get()
                        .init(webContents, mCSPCheckerBridge.getNativeCSPChecker());
    }

    /** @return Whether the native downloader is initialized. */
    public boolean isInitialized() {
        ThreadUtils.assertOnUiThread();
        return mNativeObject != 0;
    }

    /**
     * Downloads the payment method manifest file asynchronously.
     *
     * @param merchantOrigin The origin of the iframe that invoked the PaymentRequest API.
     * @param methodName     The payment method name that is a URI with HTTPS scheme.
     * @param callback       The callback to invoke when finished downloading.
     */
    public void downloadPaymentMethodManifest(
            Origin merchantOrigin, GURL methodName, ManifestDownloadCallback callback) {
        ThreadUtils.assertOnUiThread();
        assert mNativeObject != 0;
        assert merchantOrigin != null;
        PaymentManifestDownloaderJni.get()
                .downloadPaymentMethodManifest(
                        mNativeObject,
                        PaymentManifestDownloader.this,
                        merchantOrigin,
                        methodName,
                        callback);
    }

    /**
     * Downloads the web app manifest file asynchronously.
     *
     * @param paymentMethodManifestOrigin The origin of the payment method manifest that is pointing
     *                                    to this web app manifest.
     * @param webAppManifestUrl           The web app manifest URL with HTTPS scheme.
     * @param callback                    The callback to invoke when finished downloading.
     */
    public void downloadWebAppManifest(
            Origin paymentMethodManifestOrigin,
            GURL webAppManifestUrl,
            ManifestDownloadCallback callback) {
        ThreadUtils.assertOnUiThread();
        assert mNativeObject != 0;
        assert paymentMethodManifestOrigin != null;
        PaymentManifestDownloaderJni.get()
                .downloadWebAppManifest(
                        mNativeObject,
                        PaymentManifestDownloader.this,
                        paymentMethodManifestOrigin,
                        webAppManifestUrl,
                        callback);
    }

    /** Destroys the native downloader. */
    public void destroy() {
        ThreadUtils.assertOnUiThread();
        assert mNativeObject != 0;
        PaymentManifestDownloaderJni.get().destroy(mNativeObject, PaymentManifestDownloader.this);
        mNativeObject = 0;
        if (mCSPCheckerBridge != null) mCSPCheckerBridge.destroy();
    }

    /** @return An opaque origin to be used in tests. */
    public static Origin createOpaqueOriginForTest() {
        return PaymentManifestDownloaderJni.get().createOpaqueOriginForTest();
    }

    @NativeMethods
    interface Natives {
        long init(WebContents webContents, long nativeCSPCheckerAndroid);

        void downloadPaymentMethodManifest(
                long nativePaymentManifestDownloaderAndroid,
                PaymentManifestDownloader caller,
                Origin merchantOrigin,
                GURL methodName,
                ManifestDownloadCallback callback);

        void downloadWebAppManifest(
                long nativePaymentManifestDownloaderAndroid,
                PaymentManifestDownloader caller,
                Origin paymentMethodManifestOrigin,
                GURL webAppManifestUri,
                ManifestDownloadCallback callback);

        void destroy(long nativePaymentManifestDownloaderAndroid, PaymentManifestDownloader caller);

        Origin createOpaqueOriginForTest();
    }
}
