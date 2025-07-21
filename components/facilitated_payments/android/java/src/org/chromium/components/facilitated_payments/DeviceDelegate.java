// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.facilitated_payments;

import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.net.Uri;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.PackageUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** A JNI bridge to allow the native side to interact with the Android device. */
@JNINamespace("payments::facilitated")
@NullMarked
public class DeviceDelegate {
    private static final String A2A_INTENT_ACTION_NAME =
            "org.chromium.intent.action.FACILITATED_PAYMENT";
    private static final String GOOGLE_WALLET_PACKAGE_NAME = "com.google.android.apps.walletnfcrel";
    // Deeplink to the Pix account linking page on Google Wallet.
    private static final String GOOGLE_WALLET_ADD_PIX_ACCOUNT_LINK =
            "https://wallet.google.com/gw/app/addbankaccount?utm_source=chrome&email=%s";
    // Minimum Google Wallet version that supports Pix account linking.
    private static final long PIX_MIN_SUPPORTED_WALLET_VERSION = 931593518;

    private DeviceDelegate() {}

    /**
     * The Pix account linking prompt redirects to the Google Wallet app on acceptance. Checks if
     * Wallet is eligible for Pix account linking.
     *
     * @return True if Google Wallet is installed, and the Wallet version supports Pix account
     *     linking.
     */
    @CalledByNative
    private static boolean isWalletEligibleForPixAccountLinking() {
        PackageInfo walletPackageInfo =
                PackageUtils.getPackageInfo(GOOGLE_WALLET_PACKAGE_NAME, /* flags= */ 0);

        // {@link PackageInfo} is null if the package is not installed.
        if (walletPackageInfo == null) {
            return false;
        }
        // Verify Google Wallet version supports Pix account linking.
        return PackageUtils.packageVersionCode(walletPackageInfo)
                >= PIX_MIN_SUPPORTED_WALLET_VERSION;
    }

    @CalledByNative
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static void openPixAccountLinkingPageInWallet(WindowAndroid windowAndroid, String email) {
        if (windowAndroid == null) {
            return;
        }
        Context context = windowAndroid.getContext().get();
        if (context == null) {
            return;
        }
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setData(Uri.parse(String.format(GOOGLE_WALLET_ADD_PIX_ACCOUNT_LINK, email)));
        intent.setPackage(GOOGLE_WALLET_PACKAGE_NAME);
        try {
            context.startActivity(intent);
        } catch (ActivityNotFoundException e) {
            // TODO(crbug.com/419108993): Log metrics.
        }
    }

    @CalledByNative
    static ResolveInfo[] getSupportedPaymentApps(GURL paymentLinkUrl, WindowAndroid windowAndroid) {
        if (windowAndroid == null) {
            return new ResolveInfo[0];
        }
        Context context = windowAndroid.getContext().get();
        if (context == null) {
            return new ResolveInfo[0];
        }
        PackageManager packageManager = context.getPackageManager();
        if (packageManager == null) {
            return new ResolveInfo[0];
        }
        return getSupportedPaymentApps(
                paymentLinkUrl, windowAndroid, new PackageManagerDelegate(packageManager));
    }

    @VisibleForTesting
    static ResolveInfo[] getSupportedPaymentApps(
            GURL paymentLinkUrl,
            WindowAndroid windowAndroid,
            PackageManagerDelegate packageManagerDelegate) {
        Intent searchIntent = new Intent();
        searchIntent.setAction(A2A_INTENT_ACTION_NAME);
        searchIntent.setData(Uri.parse(paymentLinkUrl.getSpec()));
        List<ResolveInfo> resolveInfos =
                packageManagerDelegate.getActivitiesThatCanRespondToIntent(searchIntent);
        // Deduplicate ResolveInfos for a same package.
        Map<String, ResolveInfo> packageToResolveInfo = new HashMap<>();
        for (ResolveInfo ri : resolveInfos) {
            if (isValidResolveInfo(ri, packageManagerDelegate)) {
                packageToResolveInfo.put(ri.activityInfo.packageName, ri);
            }
        }
        return packageToResolveInfo.values().toArray(new ResolveInfo[packageToResolveInfo.size()]);
    }

    private static boolean isValidResolveInfo(
            ResolveInfo ri, PackageManagerDelegate packageManagerDelegate) {
        if (ri == null) {
            return false;
        }
        if (ri.activityInfo == null) {
            return false;
        }
        if (ri.activityInfo.packageName == null || ri.activityInfo.packageName.isEmpty()) {
            return false;
        }
        if (ri.activityInfo.name == null || ri.activityInfo.name.isEmpty()) {
            return false;
        }
        CharSequence appName = packageManagerDelegate.getAppLabel(ri);
        if (appName == null || appName.toString().trim().isEmpty()) {
            return false;
        }
        if (packageManagerDelegate.getAppIcon(ri) == null) {
            return false;
        }
        return true;
    }
}
