// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.facilitated_payments;

import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.os.Build;
import android.provider.Settings;
import android.view.inputmethod.InputMethodInfo;
import android.view.inputmethod.InputMethodManager;

import androidx.annotation.ChecksSdkIntAtLeast;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.PackageUtils;
import org.chromium.base.TimeUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** A JNI bridge to allow the native side to interact with the Android device. */
@JNINamespace("payments::facilitated")
@NullMarked
public class DeviceDelegate {
    private static final int A2A_TRANSACTION_OUTCOME_SUCCEED = 1;
    private static final int A2A_TRANSACTION_OUTCOME_CANCELED = 2;
    private static final int A2A_TRANSACTION_OUTCOME_FAILED = 3;
    private static final String A2A_TRANSACTION_OUTCOME = "A2A_TRANSACTION_OUTCOME";
    private static final String CANCELED = "Canceled";
    private static final String UNKNOWN = "Unknown";
    private static final String SUCCEEDED = "Succeeded";
    private static final String FAILED = "Failed";
    private static final String A2A_INTENT_ACTION_NAME =
            "org.chromium.intent.action.FACILITATED_PAYMENT";
    private static final String GOOGLE_WALLET_PACKAGE_NAME = "com.google.android.apps.walletnfcrel";
    // Deeplink to the Pix account linking page on Google Wallet.
    private static final String GOOGLE_WALLET_ADD_PIX_ACCOUNT_LINK =
            "https://wallet.google.com/gw/app/addbankaccount?utm_source=chrome&email=%s";
    // Minimum Google Wallet version that supports Pix account linking.
    private static final long PIX_MIN_SUPPORTED_WALLET_VERSION = 932848136;
    private static final String GBOARD_PACKAGE_NAME = "com.google.android.inputmethod.latin";

    private DeviceDelegate() {}

    /**
     * The Pix account linking prompt redirects to the Google Wallet app on acceptance. Checks if
     * Wallet is eligible for Pix account linking.
     *
     * @return An {@link WalletEligibilityForPixAccountLinking} indicating eligibility.
     */
    @CalledByNative
    private static @WalletEligibilityForPixAccountLinking int
            getWalletEligibilityForPixAccountLinking() {
        PackageInfo walletPackageInfo =
                PackageUtils.getPackageInfo(GOOGLE_WALLET_PACKAGE_NAME, /* flags= */ 0);

        // {@link PackageInfo} is null if the package is not installed.
        if (walletPackageInfo == null) {
            return WalletEligibilityForPixAccountLinking.WALLET_NOT_INSTALLED;
        }
        // Verify Google Wallet version supports Pix account linking.
        if (PackageUtils.packageVersionCode(walletPackageInfo) < PIX_MIN_SUPPORTED_WALLET_VERSION) {
            return WalletEligibilityForPixAccountLinking.WALLET_VERSION_NOT_SUPPORTED;
        }
        return WalletEligibilityForPixAccountLinking.ELIGIBLE;
    }

    @CalledByNative
    @VisibleForTesting
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

    /**
     * Invokes a payment app by launching an intent.
     *
     * @param packageName The package name of the payment app.
     * @param activityName The activity name of the payment app.
     * @param paymentLinkScheme The payment link url scheme for the payment app.
     * @param paymentLinkUrl The payment link URL to be included as data in the intent.
     * @param windowAndroid The {@link WindowAndroid} for launching the intent.
     * @return True if the intent was shown successfully, false otherwise.
     */
    @CalledByNative
    static boolean invokePaymentApp(
            String packageName,
            String activityName,
            String paymentLinkScheme,
            GURL paymentLinkUrl,
            WindowAndroid windowAndroid) {
        if (windowAndroid == null) {
            return false;
        }
        Intent intent = new Intent();
        intent.setAction(A2A_INTENT_ACTION_NAME);
        intent.setData(Uri.parse(paymentLinkUrl.getSpec()));
        intent.setClassName(packageName, activityName);
        // showIntent returns true if the intent was shown successfully.
        // TODO(crbug.com/432821264): Handle returned transaction result and specify the `errorId`.
        return windowAndroid.showIntent(
                intent,
                (resultCode, resultIntent) ->
                        onInvokePaymentAppCallback(
                                resultCode,
                                resultIntent,
                                TimeUtils.elapsedRealtimeMillis(),
                                paymentLinkScheme),
                /* errorId= */ null);
    }

    /**
     * Checks if Pix payment is supported on the device via Gboard.
     *
     * @param windowAndroid The current {@link WindowAndroid}.
     * @return True if Gboard is the current IME, false otherwise.
     */
    @CalledByNative
    static boolean isPixSupportAvailableViaGboard(WindowAndroid windowAndroid) {
        // For versions below Android T, Pix is not supported via Gboard.
        if (!isAtLeastT()) {
            return false;
        }
        if (windowAndroid == null) {
            return false;
        }
        Context context = windowAndroid.getContext().get();
        if (context == null) {
            return false;
        }
        return GBOARD_PACKAGE_NAME.equals(getDefaultImePackageName(context));
    }

    private static String getDefaultImePackageName(Context context) {
        if (isAtLeastU()) {
            InputMethodManager imm = context.getSystemService(InputMethodManager.class);
            if (imm == null) {
                return "";
            }
            InputMethodInfo currentIme = imm.getCurrentInputMethodInfo();
            if (currentIme == null) {
                return "";
            }
            return currentIme.getPackageName();
        } else {
            String currentImeId =
                    Settings.Secure.getString(
                            context.getContentResolver(), Settings.Secure.DEFAULT_INPUT_METHOD);
            if (currentImeId == null || currentImeId.isEmpty()) {
                return "";
            }
            ComponentName componentName = ComponentName.unflattenFromString(currentImeId);
            if (componentName == null) {
                return "";
            }
            return componentName.getPackageName();
        }
    }

    @ChecksSdkIntAtLeast(api = Build.VERSION_CODES.TIRAMISU)
    private static boolean isAtLeastT() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU;
    }

    @ChecksSdkIntAtLeast(api = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    private static boolean isAtLeastU() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE;
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

    private static void onInvokePaymentAppCallback(
            int resultCode,
            @Nullable Intent resultIntent,
            long startTimeMs,
            String paymentLinkScheme) {
        long lapsedTimeMs = TimeUtils.elapsedRealtimeMillis() - startTimeMs;
        StringBuilder histogramNameBuilder = new StringBuilder("FacilitatedPayments.A2A.");

        if (resultCode == Activity.RESULT_OK) {
            int transactionOutcomeCode = -1;
            if (resultIntent != null && resultIntent.getExtras() != null) {
                transactionOutcomeCode =
                        resultIntent
                                .getExtras()
                                .getInt(A2A_TRANSACTION_OUTCOME, transactionOutcomeCode);
            }
            switch (transactionOutcomeCode) {
                case A2A_TRANSACTION_OUTCOME_SUCCEED:
                    histogramNameBuilder.append(SUCCEEDED);
                    break;
                case A2A_TRANSACTION_OUTCOME_CANCELED:
                    histogramNameBuilder.append(CANCELED);
                    break;
                case A2A_TRANSACTION_OUTCOME_FAILED:
                    histogramNameBuilder.append(FAILED);
                    break;
                default:
                    histogramNameBuilder.append(UNKNOWN);
                    break;
            }
        } else if (resultCode == Activity.RESULT_CANCELED) {
            histogramNameBuilder.append(CANCELED);
        } else {
            histogramNameBuilder.append(UNKNOWN);
        }

        histogramNameBuilder.append(".Latency");
        RecordHistogram.recordTimesHistogram(histogramNameBuilder.toString(), lapsedTimeMs);

        histogramNameBuilder.append(".").append(paymentLinkScheme);
        RecordHistogram.recordTimesHistogram(histogramNameBuilder.toString(), lapsedTimeMs);
    }
}
