// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import android.content.Intent;
import android.content.pm.ResolveInfo;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.util.Pair;

import org.chromium.components.payments.intent.WebPaymentIntentHelper;

import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Looks up installed third party Android payment apps. */
public class AndroidPaymentAppFactory implements PaymentAppFactoryInterface {
    // PaymentAppFactoryInterface implementation.
    @Override
    public void create(PaymentAppFactoryDelegate delegate) {
        AndroidPaymentAppFinder finder =
                new AndroidPaymentAppFinder(
                        new PaymentManifestWebDataService(delegate.getParams().getWebContents()),
                        new PaymentManifestDownloader(),
                        new PaymentManifestParser(),
                        new PackageManagerDelegate(),
                        delegate,
                        /* factory= */ this);
        finder.findAndroidPaymentApps();
    }

    /**
     * Checks whether there are Android payment apps on device.
     *
     * @return True if there are Android payment apps on device.
     */
    public static boolean hasAndroidPaymentApps() {
        PackageManagerDelegate packageManagerDelegate = new PackageManagerDelegate();
        // Note that all Android payment apps must support org.chromium.intent.action.PAY action
        // without additional data to be detected.
        Intent payIntent = new Intent(WebPaymentIntentHelper.ACTION_PAY);
        return !packageManagerDelegate.getActivitiesThatCanRespondToIntent(payIntent).isEmpty();
    }

    /**
     * Gets Android payments apps' information on device.
     *
     * @return Map of Android payment apps' package names to their information. Each entry of the
     *         map represents an app and the value stores its name and icon.
     */
    public static Map<String, Pair<String, Drawable>> getAndroidPaymentAppsInfo() {
        Map<String, Pair<String, Drawable>> paymentAppsInfo = new HashMap<>();

        PackageManagerDelegate packageManagerDelegate = new PackageManagerDelegate();
        Intent payIntent = new Intent(WebPaymentIntentHelper.ACTION_PAY);
        List<ResolveInfo> matches =
                packageManagerDelegate.getActivitiesThatCanRespondToIntent(payIntent);
        if (matches.isEmpty()) return paymentAppsInfo;

        for (ResolveInfo match : matches) {
            CharSequence label = packageManagerDelegate.getAppLabel(match);
            if (TextUtils.isEmpty(label)) continue;
            Pair<String, Drawable> appInfo =
                    new Pair<>(label.toString(), packageManagerDelegate.getAppIcon(match));
            paymentAppsInfo.put(match.activityInfo.packageName, appInfo);
        }

        return paymentAppsInfo;
    }
}
