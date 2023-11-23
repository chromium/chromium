// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.content_public.browser.WebContents;

/** Java wrapper of the payment manifest web data service. */
@JNINamespace("payments")
public class PaymentManifestWebDataService {
    /** Interface for the callback to invoke when getting data from the web data service. */
    public interface PaymentManifestWebDataServiceCallback {
        /**
         * Called when getPaymentMethodManifest success.
         *
         * @param appIdentifiers The list of package names and origins of the supported apps in the
         *                       payment method manifest. May also contain "*" to indicate that
         *                       all origins are supported.
         */
        @CalledByNative("PaymentManifestWebDataServiceCallback")
        void onPaymentMethodManifestFetched(String[] appIdentifiers);

        /**
         * Called when getPaymentWebAppManifest success.
         *
         * @param manifest The web app manifest sections.
         */
        @CalledByNative("PaymentManifestWebDataServiceCallback")
        void onPaymentWebAppManifestFetched(WebAppManifestSection[] manifest);
    }

    /** Holds the native counterpart of this class object. */
    private long mManifestWebDataServiceAndroid;

    /**
     * Creates a new PaymentManifestWebDataService.
     *
     * @param webContents The web contents where the payment is being requested.
     */
    public PaymentManifestWebDataService(WebContents webContents) {
        if (webContents == null || webContents.isDestroyed()) return;

        mManifestWebDataServiceAndroid =
                PaymentManifestWebDataServiceJni.get()
                        .init(PaymentManifestWebDataService.this, webContents);
    }

    /** Destroy this class object. It destroys the native counterpart. */
    public void destroy() {
        if (mManifestWebDataServiceAndroid == 0) return;

        PaymentManifestWebDataServiceJni.get()
                .destroy(mManifestWebDataServiceAndroid, PaymentManifestWebDataService.this);
        mManifestWebDataServiceAndroid = 0;
    }

    /**
     * Gets the payment method's manifest.
     *
     * @param methodName The payment method name.
     * @param callback   The callback to invoke when finishing the request.
     * @return True if the result will be returned through callback.
     */
    public boolean getPaymentMethodManifest(
            String methodName, PaymentManifestWebDataServiceCallback callback) {
        if (mManifestWebDataServiceAndroid == 0) return false;

        return PaymentManifestWebDataServiceJni.get()
                .getPaymentMethodManifest(
                        mManifestWebDataServiceAndroid,
                        PaymentManifestWebDataService.this,
                        methodName,
                        callback);
    }

    /**
     * Gets the corresponding payment web app's manifest.
     *
     * @param appPackageName The package name of the Android payment app.
     * @param callback       The callback to invoke when finishing the request.
     * @return True if the result will be returned through callback.
     */
    public boolean getPaymentWebAppManifest(
            String appPackageName, PaymentManifestWebDataServiceCallback callback) {
        if (mManifestWebDataServiceAndroid == 0) return false;

        return PaymentManifestWebDataServiceJni.get()
                .getPaymentWebAppManifest(
                        mManifestWebDataServiceAndroid,
                        PaymentManifestWebDataService.this,
                        appPackageName,
                        callback);
    }

    /**
     * Adds the supported Android apps' package names of the method.
     *
     * @param methodName       The method name.
     * @param appPackageNames  The supported app package names and origins. Also possibly "*" if
     *                         applicable.
     */
    public void addPaymentMethodManifest(String methodName, String[] appIdentifiers) {
        if (mManifestWebDataServiceAndroid == 0) return;

        PaymentManifestWebDataServiceJni.get()
                .addPaymentMethodManifest(
                        mManifestWebDataServiceAndroid,
                        PaymentManifestWebDataService.this,
                        methodName,
                        appIdentifiers);
    }

    /**
     * Adds web app's manifest.
     *
     * @param manifest The manifest.
     */
    public void addPaymentWebAppManifest(WebAppManifestSection[] manifest) {
        if (mManifestWebDataServiceAndroid == 0) return;

        PaymentManifestWebDataServiceJni.get()
                .addPaymentWebAppManifest(
                        mManifestWebDataServiceAndroid,
                        PaymentManifestWebDataService.this,
                        manifest);
    }

    @CalledByNative
    private static WebAppManifestSection[] createManifest(int numberOfsections) {
        return new WebAppManifestSection[numberOfsections];
    }

    @CalledByNative
    private static void addSectionToManifest(
            WebAppManifestSection[] manifest,
            int sectionIndex,
            String id,
            long minVersion,
            int numberOfFingerprints) {
        manifest[sectionIndex] = new WebAppManifestSection(id, minVersion, numberOfFingerprints);
    }

    @CalledByNative
    private static void addFingerprintToSection(
            WebAppManifestSection[] manifest,
            int sectionIndex,
            int fingerprintIndex,
            byte[] fingerprint) {
        manifest[sectionIndex].fingerprints[fingerprintIndex] = fingerprint;
    }

    @CalledByNative
    private static String getIdFromSection(WebAppManifestSection manifestSection) {
        return manifestSection.id;
    }

    @CalledByNative
    private static long getMinVersionFromSection(WebAppManifestSection manifestSection) {
        return manifestSection.minVersion;
    }

    @CalledByNative
    private static byte[][] getFingerprintsFromSection(WebAppManifestSection manifestSection) {
        return manifestSection.fingerprints;
    }

    @NativeMethods
    interface Natives {
        long init(PaymentManifestWebDataService caller, WebContents webContents);

        void destroy(
                long nativePaymentManifestWebDataServiceAndroid,
                PaymentManifestWebDataService caller);

        boolean getPaymentMethodManifest(
                long nativePaymentManifestWebDataServiceAndroid,
                PaymentManifestWebDataService caller,
                String methodName,
                PaymentManifestWebDataServiceCallback callback);

        boolean getPaymentWebAppManifest(
                long nativePaymentManifestWebDataServiceAndroid,
                PaymentManifestWebDataService caller,
                String appPackageName,
                PaymentManifestWebDataServiceCallback callback);

        void addPaymentMethodManifest(
                long nativePaymentManifestWebDataServiceAndroid,
                PaymentManifestWebDataService caller,
                String methodName,
                String[] appPackageNames);

        void addPaymentWebAppManifest(
                long nativePaymentManifestWebDataServiceAndroid,
                PaymentManifestWebDataService caller,
                WebAppManifestSection[] manifest);
    }
}
