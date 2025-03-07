// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

/**
 * An installer for fake Android payment apps for testing. Can install either one or more than one
 * payment app.
 *
 * <p>Installing fake Android payment apps affects global state, in {@link AndroidPaymentAppFinder}.
 *
 * <p>The first payment app's package name is "test.payments.app". Its signature is
 * "AABBCCDDEEFF001122334455".
 *
 * <p>If multiple payment apps will be installed, then the second payment app's package name is
 * "test.payments.other.app". Its signature is "001122334455AABBCCDDEEFF".
 */
public class MockPaymentAppInstaller {
    private final String mPaymentMethodName;
    private final String mOtherPaymentMethodName;

    /**
     * Constructs a fake payment app installer.
     *
     * @param paymentMethodName The method name for the first Android payment app, e.g.,
     *     "https://payments.test/web-pay".
     * @param otherPaymentMethodName The method name for the second Android payment app, if multiple
     *     payment apps will be installed, e.g., "https://other-payments.example/web-pay".
     */
    public MockPaymentAppInstaller(String paymentMethodName, String otherPaymentMethodName) {
        mPaymentMethodName = paymentMethodName;
        mOtherPaymentMethodName = otherPaymentMethodName;
    }

    /**
     * Injects a fake Android payment app into the package manager delegate, with the correct
     * signature being returned from the downloader. Also turns off connecting to the
     * IS_READY_TO_PAY service or sending the PAY intent to this app.
     *
     * @param multipleApps Whether multiple apps should be installed.
     */
    public void installPaymentApps(boolean multipleApps) {
        MockPackageManagerDelegate packageManagerDelegate = new MockPackageManagerDelegate();
        // The SHA256 of the string "AABBCCDDEEFF001122334455" equals to the fingerprints[0].value
        // in the "downloaded" manifest file.
        packageManagerDelegate.installPaymentApp(
                "Test Payment App",
                "test.payments.app",
                mPaymentMethodName,
                "AABBCCDDEEFF001122334455");
        if (multipleApps) {
            // The SHA256 of the string "001122334455AABBCCDDEEFF" equals to the
            // fingerprints[0].value in the "downloaded" manifest file.
            packageManagerDelegate.installPaymentApp(
                    "Other Test Payment App",
                    "test.payments.other.app",
                    mOtherPaymentMethodName,
                    "001122334455AABBCCDDEEFF");
        }
        AndroidPaymentAppFinder.setPackageManagerDelegateForTest(packageManagerDelegate);
        AndroidPaymentAppFinder.setDownloaderForTest(
                new MockPaymentManifestDownloader(mPaymentMethodName));
        AndroidPaymentAppFinder.setAndroidIntentLauncherForTest(
                new MockAndroidIntentLauncher(
                        /* returnShippingAddress= */ false, /* returnContactInfo= */ false));
        AndroidPaymentAppFinder.bypassIsReadyToPayServiceInTest();
    }

    /**
     * Resets the Android payment app finder to its own original state of querying the Android
     * package manager and downloading manifest files from the web.
     */
    public void reset() {
        AndroidPaymentAppFinder.setPackageManagerDelegateForTest(null);
        AndroidPaymentAppFinder.setDownloaderForTest(null);
        AndroidPaymentAppFinder.setAndroidIntentLauncherForTest(null);
    }
}
