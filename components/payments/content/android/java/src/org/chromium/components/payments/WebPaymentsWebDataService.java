// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.content_public.browser.WebContents;

/** Java wrapper of the Web Payments web data service. */
@JNINamespace("payments")
@NullMarked
public class WebPaymentsWebDataService {
    /** Interface for the callback to invoke when getting data from the web data service. */
    public interface WebPaymentsWebDataServiceCallback {
        /**
         * Called when getPaymentMethodManifest success.
         *
         * @param appIdentifiers The list of package names and origins of the supported apps in the
         *     payment method manifest. May also contain "*" to indicate that all origins are
         *     supported.
         */
        @CalledByNative("WebPaymentsWebDataServiceCallback")
        void onPaymentMethodManifestFetched(String[] appIdentifiers);

        /**
         * Called when getPaymentWebAppManifest success.
         *
         * @param manifest The web app manifest sections.
         */
        @CalledByNative("WebPaymentsWebDataServiceCallback")
        void onPaymentWebAppManifestFetched(WebAppManifestSection[] manifest);
    }

    /** Holds the native counterpart of this class object. */
    private long mManifestWebDataServiceAndroid;

    /**
     * Creates a new WebPaymentsWebDataService.
     *
     * @param webContents The web contents where the payment is being requested.
     */
    public WebPaymentsWebDataService(WebContents webContents) {
        if (webContents == null || webContents.isDestroyed()) return;

        mManifestWebDataServiceAndroid =
                WebPaymentsWebDataServiceJni.get()
                        .init(WebPaymentsWebDataService.this, webContents);
    }

    /** Destroy this class object. It destroys the native counterpart. */
    public void destroy() {
        if (mManifestWebDataServiceAndroid == 0) return;

        WebPaymentsWebDataServiceJni.get().destroy(mManifestWebDataServiceAndroid);
        mManifestWebDataServiceAndroid = 0;
    }

    /**
     * Gets the payment method's manifest.
     *
     * @param methodName The payment method name.
     * @param callback The callback to invoke when finishing the request.
     * @return True if the result will be returned through callback.
     */
    public boolean getPaymentMethodManifest(
            String methodName, WebPaymentsWebDataServiceCallback callback) {
        if (mManifestWebDataServiceAndroid == 0) return false;

        return WebPaymentsWebDataServiceJni.get()
                .getPaymentMethodManifest(mManifestWebDataServiceAndroid, methodName, callback);
    }

    /**
     * Gets the corresponding payment web app's manifest.
     *
     * @param appPackageName The package name of the Android payment app.
     * @param callback The callback to invoke when finishing the request.
     * @return True if the result will be returned through callback.
     */
    public boolean getPaymentWebAppManifest(
            String appPackageName, WebPaymentsWebDataServiceCallback callback) {
        if (mManifestWebDataServiceAndroid == 0) return false;

        return WebPaymentsWebDataServiceJni.get()
                .getPaymentWebAppManifest(mManifestWebDataServiceAndroid, appPackageName, callback);
    }

    /**
     * Adds the supported Android apps' package names of the method.
     *
     * @param methodName The method name.
     * @param appPackageNames The supported app package names and origins. Also possibly "*" if
     *     applicable.
     */
    public void addPaymentMethodManifest(String methodName, String[] appIdentifiers) {
        if (mManifestWebDataServiceAndroid == 0) return;

        WebPaymentsWebDataServiceJni.get()
                .addPaymentMethodManifest(
                        mManifestWebDataServiceAndroid, methodName, appIdentifiers);
    }

    /**
     * Adds web app's manifest.
     *
     * @param manifest The manifest.
     */
    public void addPaymentWebAppManifest(WebAppManifestSection[] manifest) {
        if (mManifestWebDataServiceAndroid == 0) return;

        WebPaymentsWebDataServiceJni.get()
                .addPaymentWebAppManifest(mManifestWebDataServiceAndroid, manifest);
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
        long init(WebPaymentsWebDataService caller, WebContents webContents);

        void destroy(long nativeWebPaymentsWebDataServiceAndroid);

        boolean getPaymentMethodManifest(
                long nativeWebPaymentsWebDataServiceAndroid,
                String methodName,
                WebPaymentsWebDataServiceCallback callback);

        boolean getPaymentWebAppManifest(
                long nativeWebPaymentsWebDataServiceAndroid,
                String appPackageName,
                WebPaymentsWebDataServiceCallback callback);

        void addPaymentMethodManifest(
                long nativeWebPaymentsWebDataServiceAndroid,
                String methodName,
                String[] appPackageNames);

        void addPaymentWebAppManifest(
                long nativeWebPaymentsWebDataServiceAndroid, WebAppManifestSection[] manifest);
    }
}
