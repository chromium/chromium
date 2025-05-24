// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import org.chromium.components.payments.MockPackageManagerDelegate.PackageInfoState;

/**
 * A mock Android payment app, used for testing, together with {@link MockPaymentAppInstaller}. See
 * the {@link MockPaymentAppInstaller} class comment to see how this mock app can be installed.
 *
 * <p>Sample usage:
 *
 * <pre>
 *   new MockPaymentApp()
 *           .setLabel("Test Payments App")
 *           .setPackage("test.payments.app")
 *           .setMethod("https://payments.test/web-pay")
 *           .setSignature("AABBCCDDEEFF001122334455")
 *           .setSha256CertificateFingerprint("79:5C:8E:4D:57:7B:76:49:3A:0A:0B:93:B9:BE")
 *           .setPackageInfoState(PackageInfoState.ONE_VALID_SIGNATURE)
 *           .setHandlesShippingAddress()
 *           .setHandlesContactInformation();
 * </pre>
 */
public class MockPaymentApp {
    private String mLabel;
    private String mPackage;
    private String mMethod;
    private String mSignature;
    private String mSha256CertificateFingerprint;
    private PackageInfoState mPackageInfoState = PackageInfoState.ONE_VALID_SIGNATURE;
    private boolean mHandlesShippingAddress;
    private boolean mHandlesContactInformation;

    /**
     * @param label The app's user visible label, e.g., "Test Payments App". Must be non-empty for
     *     the {@link AndroidPaymentAppFinder} to consider this app to be valid.
     * @return A reference to this {@link MockPaymentApp} instance.
     */
    public MockPaymentApp setLabel(String label) {
        mLabel = label;
        return this;
    }

    /**
     * @param packageName The app's Android package name, e.g., "test.payments.app". The {@link
     *     PaymentManifestVerifier} will look for this package name in the "related_applications"
     *     section of the webapp manifest file of this app.
     * @return A reference to this {@link MockPaymentApp} instance.
     */
    public MockPaymentApp setPackage(String packageName) {
        mPackage = packageName;
        return this;
    }

    /**
     * @param method The app's default payment method name, e.g., "https://payments.test/web-pay".
     *     The {@link PaymentManifestVerifier} will perform an HTTP HEAD request on this URL to look
     *     up the webapp manifest file for this app.
     * @return A reference to this {@link MockPaymentApp} instance.
     */
    public MockPaymentApp setMethod(String method) {
        mMethod = method;
        return this;
    }

    /**
     * @param signature The app's APK signature value, e.g., "AABBCCDDEEFF001122334455". The {@link
     *     PaymentManifestVerifier} will look for the SHA256 value of this string in the
     *     "related_applications" section of the webapp manifest file of this app.
     * @return A reference to this {@link MockPaymentApp} instance.
     */
    public MockPaymentApp setSignature(String signature) {
        mSignature = signature;
        return this;
    }

    /**
     * @param sha256CertificateFingerprint The SHA256 certificate fingerprint of this app's
     *     signature.
     * @return A reference to this {@link MockPaymentApp} instance.
     */
    public MockPaymentApp setSha256CertificateFingerprint(String sha256CertificateFingerprint) {
        mSha256CertificateFingerprint = sha256CertificateFingerprint;
        return this;
    }

    /**
     * @param packageInfostate The state of the package info and the signature in it.
     * @return A reference to this {@link MockPaymentApp} instance.
     */
    public MockPaymentApp setPackageInfoState(PackageInfoState packageInfoState) {
        mPackageInfoState = packageInfoState;
        return this;
    }

    /**
     * Enables this mock payment app to provide a shipping address.
     *
     * @return A reference to this {@link MockPaymentApp} instance.
     */
    public MockPaymentApp setHandlesShippingAddress() {
        mHandlesShippingAddress = true;
        return this;
    }

    /**
     * Enables this mock payment app to provide contact information.
     *
     * @return A reference to this {@link MockPaymentApp} instance.
     */
    public MockPaymentApp setHandlesContactInformation() {
        mHandlesContactInformation = true;
        return this;
    }

    /**
     * @return The app's user visible label, e.g., "Test Payments App".
     */
    public String getLabel() {
        return mLabel;
    }

    /**
     * @return The app's Android package name, e.g., "test.payments.app".
     */
    public String getPackage() {
        return mPackage;
    }

    /**
     * @return The app's default payment method name, e.g., "https://payments.test/web-pay".
     */
    public String getMethod() {
        return mMethod;
    }

    /**
     * @return The app's APK signature value, e.g., "AABBCCDDEEFF001122334455".
     */
    public String getSignature() {
        return mSignature;
    }

    /**
     * @return The SHA256 certificate fingerprint of this app's signature.
     */
    public String getSha256CertificateFingerprint() {
        return mSha256CertificateFingerprint;
    }

    /**
     * @return The state of the package info and the signature in it.
     */
    public PackageInfoState getPackageInfoState() {
        return mPackageInfoState;
    }

    /**
     * @return Whether this mock payment app can provide a shipping address.
     */
    public boolean getHandlesShippingAddress() {
        return mHandlesShippingAddress;
    }

    /**
     * @return Whether this mock payment app can provide contact information.
     */
    public boolean getHandlesContactInformation() {
        return mHandlesContactInformation;
    }
}
