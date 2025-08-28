// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * An installer for fake Android payment apps for testing.
 *
 * <p>Installing fake Android payment apps affects global state, in {@link AndroidPaymentAppFinder}.
 *
 * <p>Sample usage:
 *
 * <pre>
 *    new MockPaymentAppInstaller()
 *            .addApp(new MockPaymentApp()
 *                    .setLabel("Test Payments App")
 *                    .setPackage("test.payments.app")
 *                    .setMethod("https://payments.test/web-pay")
 *                    .setSignature("AABBCCDDEEFF001122334455")
 *                    .setSha256CertificateFingerprint("79:5C:8E:4D:57:7B:76:49:3A:0A:0B:93:B9:BE")
 *                    .setHandlesShippingAddress())
 *            .addApp(new MockPaymentApp()
 *                    .setLabel("Other Test Payments App")
 *                    .setPackage("test.payments.other.app")
 *                    .setMethod("https://other-payments.example/web-pay")
 *                    .setSignature("001122334455AABBCCDDEEFF")
 *                    .setSha256CertificateFingerprint("01:9D:A6:93:7D:A2:1D:64:25:D8:D4:93:37:29")
 *                    .setHandlesContactInformation())
 *             .install();
 * </pre>
 */
public class MockPaymentAppInstaller {
    private final Set<String> mMethodNames = new HashSet<>();
    private final List<MockPaymentApp> mApps = new ArrayList<>();
    private boolean mIsReadyToPay = true;

    /**
     * Adds a mock payment app to the list of apps to be installed.
     *
     * @param app The mock payment app to add to the list of apps to be installed. Each mock payment
     *     app should have a different payment method name.
     * @return A refence to this {@link MockPaymentAppInstaller} instance.
     */
    public MockPaymentAppInstaller addApp(MockPaymentApp app) {
        assert !mMethodNames.contains(app.getMethod())
                : "Each mock payment app should have a different payment method name.";
        mMethodNames.add(app.getMethod());
        mApps.add(app);
        return this;
    }

    /**
     * Simulates the IS_READY_TO_PAY services response for all payment apps. If not set, the default
     * is true.
     *
     * @param isReadyToPay The simulated return value of the IS_READY_TO_PAY service.
     * @return A refence to this {@link MockPaymentAppInstaller} instance.
     */
    public MockPaymentAppInstaller setReadyToPay(boolean isReadyToPay) {
        mIsReadyToPay = isReadyToPay;
        return this;
    }

    /**
     * Injects the mock Android payment apps into the package manager delegate, with the correct
     * signature being returned from the downloader. Turns off connecting to the IS_READY_TO_PAY
     * service or sending the PAY intent to this app.
     */
    public void install() {
        MockPackageManagerDelegate packageManagerDelegate = new MockPackageManagerDelegate();
        MockPaymentManifestDownloader downloader = new MockPaymentManifestDownloader();
        MockAndroidIntentLauncher launcher = new MockAndroidIntentLauncher();

        for (MockPaymentApp app : mApps) {
            packageManagerDelegate.installPaymentApp(
                    app.getLabel(),
                    app.getPackage(),
                    app.getMethod(),
                    getSupportedDelegations(app),
                    app.getSignature(),
                    app.getPackageInfoState());
            if (app.hasReadyToPayService()) {
                packageManagerDelegate.addIsReadyToPayService(app.getPackage());
            }
            downloader.serveManifestFor(app);
            launcher.handleLaunchingApp(app);
        }

        AndroidPaymentAppFinder.setPackageManagerDelegateForTest(packageManagerDelegate);
        AndroidPaymentAppFinder.setDownloaderForTest(downloader);
        AndroidPaymentAppFinder.setAndroidIntentLauncherForTest(launcher);

        AndroidPaymentAppFinder.bypassIsReadyToPayServiceInTest(true);
        AndroidPaymentAppFinder.setIsReadyToPayResponseInTest(mIsReadyToPay);
    }

    private static String[] getSupportedDelegations(MockPaymentApp app) {
        List<String> delegations = new ArrayList<>();
        if (app.getHandlesShippingAddress()) {
            delegations.add("shippingAddress");
        }

        if (app.getHandlesContactInformation()) {
            delegations.add("payerName");
            delegations.add("payerEmail");
            delegations.add("payerPhone");
        }

        return delegations.isEmpty() ? null : delegations.toArray(new String[0]);
    }

    /**
     * Resets the Android payment app finder to its own original state of querying the Android
     * package manager and downloading manifest files from the web.
     */
    public void reset() {
        AndroidPaymentAppFinder.setPackageManagerDelegateForTest(null);
        AndroidPaymentAppFinder.setDownloaderForTest(null);
        AndroidPaymentAppFinder.setAndroidIntentLauncherForTest(null);
        AndroidPaymentAppFinder.bypassIsReadyToPayServiceInTest(false);
        AndroidPaymentAppFinder.setIsReadyToPayResponseInTest(true);
        mApps.clear();
    }
}
