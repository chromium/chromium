// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import android.accounts.Account;
import android.app.Activity;
import android.content.Intent;
import android.os.Build;
import android.provider.Settings;

import org.chromium.base.IntentUtils;

/**
 * Helper functions for sign-in and accounts.
 */
public final class SigninUtils {
    private static final String ACCOUNT_SETTINGS_ACTION = "android.settings.ACCOUNT_SYNC_SETTINGS";
    private static final String ACCOUNT_SETTINGS_ACCOUNT_KEY = "account";

    private SigninUtils() {}

    /**
     * Opens a Settings page to configure settings for a single account.
     * @param activity Activity to use when starting the Activity.
     * @param account The account for which the Settings page should be opened.
     * @return Whether or not Android accepted the Intent.
     */
    public static boolean openSettingsForAccount(Activity activity, Account account) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            // ACCOUNT_SETTINGS_ACTION no longer works on Android O+, always open all accounts page.
            return openSettingsForAllAccounts(activity);
        }
        Intent intent = new Intent(ACCOUNT_SETTINGS_ACTION);
        intent.putExtra(ACCOUNT_SETTINGS_ACCOUNT_KEY, account);
        return IntentUtils.safeStartActivity(activity, intent);
    }

    /**
     * Opens a Settings page with all accounts on the device.
     * @param activity Activity to use when starting the Activity.
     * @return Whether or not Android accepted the Intent.
     */
    public static boolean openSettingsForAllAccounts(Activity activity) {
        return IntentUtils.safeStartActivity(activity, new Intent(Settings.ACTION_SYNC_SETTINGS));
    }
}
