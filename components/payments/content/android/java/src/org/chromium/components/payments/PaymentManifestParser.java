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

/** Parses payment manifests in a utility process. */
@JNINamespace("payments")
public class PaymentManifestParser {
    /** Interface for the callback to invoke when finished parsing. */
    public interface ManifestParseCallback {
        /**
         * Called on successful parse of a payment method manifest.
         *
         * @param webAppManifestUris  The URLs of the default applications in the parsed manifest.
         * @param supportedOrigins    The URLs for the supported origins in the parsed manifest.
         */
        @CalledByNative("ManifestParseCallback")
        void onPaymentMethodManifestParseSuccess(
                GURL[] webAppManifestUris, GURL[] supportedOrigins);

        /**
         * Called on successful parse of a web app manifest.
         *
         * @param manifest The successfully parsed web app manifest.
         */
        @CalledByNative("ManifestParseCallback")
        void onWebAppManifestParseSuccess(WebAppManifestSection[] manifest);

        /** Called on failed parse of a payment method manifest. */
        @CalledByNative("ManifestParseCallback")
        void onManifestParseFailure();
    }

    /** Owned native host of the utility process that parses manifest contents. */
    private long mNativePaymentManifestParserAndroid;

    /**
     * Init the native side of this class.
     * Must be called before parsePaymentMethodManifest or parseWebAppManifest can be called.
     * @param webContents The web contents in whose developer console parsing errors and warnings
     *                    will be printed.
     */
    public void createNative(WebContents webContents) {
        ThreadUtils.assertOnUiThread();
        assert mNativePaymentManifestParserAndroid == 0;
        mNativePaymentManifestParserAndroid =
                PaymentManifestParserJni.get().createPaymentManifestParserAndroid(webContents);
    }

    /** Releases the resources held by the native side. */
    public void destroyNative() {
        ThreadUtils.assertOnUiThread();
        assert mNativePaymentManifestParserAndroid != 0;
        PaymentManifestParserJni.get()
                .destroyPaymentManifestParserAndroid(mNativePaymentManifestParserAndroid);
        mNativePaymentManifestParserAndroid = 0;
    }

    /** @return Whether the native side is initialized. */
    public boolean isNativeInitialized() {
        ThreadUtils.assertOnUiThread();
        return mNativePaymentManifestParserAndroid != 0;
    }

    /**
     * Parses the payment method manifest file asynchronously.
     *
     * @param manifestUrl The URL of the payment method manifest that is being parsed. Used for
     * resolving the optionally relative URL of the default application.
     * @param content The content to parse.
     * @param callback The callback to invoke when finished parsing.
     */
    public void parsePaymentMethodManifest(
            GURL manifestUrl, String content, ManifestParseCallback callback) {
        ThreadUtils.assertOnUiThread();
        assert mNativePaymentManifestParserAndroid != 0;
        PaymentManifestParserJni.get()
                .parsePaymentMethodManifest(
                        mNativePaymentManifestParserAndroid, manifestUrl, content, callback);
    }

    /**
     * Parses the web app manifest file asynchronously.
     *
     * @param content  The content to parse.
     * @param callback The callback to invoke when finished parsing.
     */
    public void parseWebAppManifest(String content, ManifestParseCallback callback) {
        ThreadUtils.assertOnUiThread();
        assert mNativePaymentManifestParserAndroid != 0;
        PaymentManifestParserJni.get()
                .parseWebAppManifest(mNativePaymentManifestParserAndroid, content, callback);
    }

    @CalledByNative
    private static GURL[] createUrlArray(int numberOfWebAppManifests) {
        return new GURL[numberOfWebAppManifests];
    }

    @CalledByNative
    private static boolean addUrl(GURL[] uris, int uriIndex, String uriToAdd) {
        GURL url = new GURL(uriToAdd);
        if (!url.isValid()) return false;

        uris[uriIndex] = new GURL(uriToAdd);
        return true;
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

    @NativeMethods
    interface Natives {
        long createPaymentManifestParserAndroid(WebContents webContents);

        void destroyPaymentManifestParserAndroid(long nativePaymentManifestParserAndroid);

        void parsePaymentMethodManifest(
                long nativePaymentManifestParserAndroid,
                GURL manifestUrl,
                String content,
                ManifestParseCallback callback);

        void parseWebAppManifest(
                long nativePaymentManifestParserAndroid,
                String content,
                ManifestParseCallback callback);
    }
}
