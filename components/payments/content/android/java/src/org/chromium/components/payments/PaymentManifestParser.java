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
import java.net.URISyntaxException;

/** Parses payment manifests in a utility process. */
@JNINamespace("payments")
public class PaymentManifestParser {
    /** Interface for the callback to invoke when finished parsing. */
    public interface ManifestParseCallback {
        /**
         * Called on successful parse of a payment method manifest.
         *
         * @param webAppManifestUris  The URIs of the default applications in the parsed manifest.
         * @param supportedOrigins    The URIs for the supported origins in the parsed manifest.
         * @param allOriginsSupported Whether all origins are supported.
         */
        @CalledByNative("ManifestParseCallback")
        void onPaymentMethodManifestParseSuccess(
                URI[] webAppManifestUris, URI[] supportedOrigins, boolean allOriginsSupported);

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
        PaymentManifestParserJni.get().destroyPaymentManifestParserAndroid(
                mNativePaymentManifestParserAndroid);
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
     * @param content  The content to parse.
     * @param callback The callback to invoke when finished parsing.
     */
    public void parsePaymentMethodManifest(String content, ManifestParseCallback callback) {
        ThreadUtils.assertOnUiThread();
        assert mNativePaymentManifestParserAndroid != 0;
        PaymentManifestParserJni.get().parsePaymentMethodManifest(
                mNativePaymentManifestParserAndroid, content, callback);
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
        PaymentManifestParserJni.get().parseWebAppManifest(
                mNativePaymentManifestParserAndroid, content, callback);
    }

    @CalledByNative
    private static URI[] createUriArray(int numberOfWebAppManifests) {
        return new URI[numberOfWebAppManifests];
    }

    @CalledByNative
    private static boolean addUri(URI[] uris, int uriIndex, String uriToAdd) {
        try {
            uris[uriIndex] = new URI(uriToAdd);
        } catch (URISyntaxException e) {
            return false;
        }
        return true;
    }

    @CalledByNative
    private static WebAppManifestSection[] createManifest(int numberOfsections) {
        return new WebAppManifestSection[numberOfsections];
    }

    @CalledByNative
    private static void addSectionToManifest(WebAppManifestSection[] manifest, int sectionIndex,
            String id, long minVersion, int numberOfFingerprints) {
        manifest[sectionIndex] = new WebAppManifestSection(id, minVersion, numberOfFingerprints);
    }

    @CalledByNative
    private static void addFingerprintToSection(WebAppManifestSection[] manifest, int sectionIndex,
            int fingerprintIndex, byte[] fingerprint) {
        manifest[sectionIndex].fingerprints[fingerprintIndex] = fingerprint;
    }

    @NativeMethods
    interface Natives {
        long createPaymentManifestParserAndroid(WebContents webContents);
        void destroyPaymentManifestParserAndroid(long nativePaymentManifestParserAndroid);
        void parsePaymentMethodManifest(long nativePaymentManifestParserAndroid, String content,
                ManifestParseCallback callback);
        void parseWebAppManifest(long nativePaymentManifestParserAndroid, String content,
                ManifestParseCallback callback);
    }
}
