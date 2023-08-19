// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import android.accounts.Account;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.provider.Settings;
import android.text.TextUtils;

import org.chromium.base.IntentUtils;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;

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

    /**
     * Return the appropriate string for 'Continue as John Doe' button, given that
     * 'Continue as john.doe@example.com' is used as a fallback and certain accounts cannot have
     * their email address displayed. In such case, use 'Continue' instead.
     *
     * @param context The Android Context used to inflate the View.
     * @param profileData Cached DisplayableProfileData containing the full name and the email
     *         address.
     * @return Appropriate string for continueButton.
     */
    public static String getContinueAsButtonText(
            final Context context, DisplayableProfileData profileData) {
        if (!TextUtils.isEmpty(profileData.getGivenName())) {
            return context.getString(R.string.sync_promo_continue_as, profileData.getGivenName());
        }
        if (!TextUtils.isEmpty(profileData.getFullName())) {
            return context.getString(R.string.sync_promo_continue_as, profileData.getFullName());
        }
        if (!profileData.hasDisplayableEmailAddress()) {
            return context.getString(R.string.sync_promo_continue);
        }
        return context.getString(R.string.sync_promo_continue_as, profileData.getAccountEmail());
    }

    /**
     * Returns the accessibility label for the the account picker.
     */
    public static String getChooseAccountLabel(
            final Context context, DisplayableProfileData profileData) {
        if (profileData.hasDisplayableEmailAddress()) {
            return context.getString(R.string.signin_account_picker_description_with_email,
                    profileData.getAccountEmail());
        }
        return context.getString(R.string.signin_account_picker_description);
    }
}
