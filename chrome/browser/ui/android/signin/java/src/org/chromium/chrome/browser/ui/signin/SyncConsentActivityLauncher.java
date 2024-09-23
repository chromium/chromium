// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import android.content.Context;

import androidx.annotation.IntDef;

import org.chromium.components.signin.metrics.SigninAccessPoint;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Allows for launching {@link SyncConsentActivity} in modularized code. */
public interface SyncConsentActivityLauncher {
    @IntDef({
        SigninAccessPoint.SETTINGS,
        SigninAccessPoint.BOOKMARK_MANAGER,
        SigninAccessPoint.RECENT_TABS,
        SigninAccessPoint.SIGNIN_PROMO,
        SigninAccessPoint.NTP_FEED_TOP_PROMO,
        SigninAccessPoint.AUTOFILL_DROPDOWN,
        SigninAccessPoint.NTP_SIGNED_OUT_ICON
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface AccessPoint {}

    /**
     * Launches the {@link SyncConsentActivity} with default sign-in flow from personalized sign-in
     * promo.
     * @param accessPoint {@link SigninAccessPoint} for starting sign-in flow.
     * @param accountName The account to preselect or null to preselect the default account.
     */
    void launchActivityForPromoDefaultFlow(
            Context context, @SigninAccessPoint int accessPoint, String accountName);

    /**
     * Launches the {@link SyncConsentActivity} with "Choose account" sign-in flow from personalized
     * sign-in promo.
     * @param accessPoint {@link SigninAccessPoint} for starting sign-in flow.
     * @param accountName The account to preselect or null to preselect the default account.
     */
    void launchActivityForPromoChooseAccountFlow(
            Context context, @SigninAccessPoint int accessPoint, String accountName);

    /**
     * Launches the {@link SyncConsentActivity} with "New account" sign-in flow from personalized
     * sign-in promo.
     * @param accessPoint {@link SigninAccessPoint} for starting sign-in flow.
     */
    void launchActivityForPromoAddAccountFlow(Context context, @SigninAccessPoint int accessPoint);

    /**
     * Launches the {@link SyncConsentActivity} if signin is allowed.
     * @param context A {@link Context} object.
     * @param accessPoint {@link SigninAccessPoint} for starting sign-in flow.
     * @return a boolean indicating if the {@link SyncConsentActivity} is launched.
     */
    boolean launchActivityIfAllowed(Context context, @SigninAccessPoint int accessPoint);
}
