// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

/** Parses payment manifests. */
@JNINamespace("payments")
@NullMarked
public class PaymentManifestParser {
    /** Container for the result of parsing a payment method manifest. */
    public static class PaymentMethodManifest {
        public final GURL[] webAppManifestUris;
        public final GURL[] supportedOrigins;

        public PaymentMethodManifest(GURL[] webAppManifestUris, GURL[] supportedOrigins) {
            this.webAppManifestUris = webAppManifestUris;
            this.supportedOrigins = supportedOrigins;
        }
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
     * Parses the payment method manifest file.
     *
     * @param manifestUrl The URL of the payment method manifest that is being parsed. Used for
     *     resolving the optionally relative URL of the default application.
     * @param content The content to parse.
     * @return The parsed manifest, or null on failure.
     */
    public @Nullable PaymentMethodManifest parsePaymentMethodManifest(
            GURL manifestUrl, String content) {
        ThreadUtils.assertOnUiThread();
        assert mNativePaymentManifestParserAndroid != 0;
        return PaymentManifestParserJni.get()
                .parsePaymentMethodManifest(
                        mNativePaymentManifestParserAndroid, manifestUrl, content);
    }

    /**
     * Parses the web app manifest file.
     *
     * @param content The content to parse.
     * @return The parsed web app manifest sections, or null on failure.
     */
    public WebAppManifestSection @Nullable [] parseWebAppManifest(String content) {
        ThreadUtils.assertOnUiThread();
        assert mNativePaymentManifestParserAndroid != 0;
        return PaymentManifestParserJni.get()
                .parseWebAppManifest(mNativePaymentManifestParserAndroid, content);
    }

    @CalledByNative
    private static PaymentMethodManifest createPaymentMethodManifest(
            GURL[] webAppManifestUris, GURL[] supportedOrigins) {
        return new PaymentMethodManifest(webAppManifestUris, supportedOrigins);
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

        @Nullable PaymentMethodManifest parsePaymentMethodManifest(
                long nativePaymentManifestParserAndroid, GURL manifestUrl, String content);

        WebAppManifestSection @Nullable [] parseWebAppManifest(
                long nativePaymentManifestParserAndroid, String content);
    }
}
