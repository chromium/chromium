// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.res.Configuration;
import android.os.Build;
import android.provider.Settings;
import android.text.TextUtils;
import android.view.View;

import org.chromium.base.BuildInfo;
import org.chromium.base.IntentUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.components.signin.AccountUtils;
import org.chromium.ui.base.DeviceFormFactor;

/** Helper functions for sign-in and accounts. */
public final class SigninUtils {
    private static final String ACCOUNT_SETTINGS_ACTION = "android.settings.ACCOUNT_SYNC_SETTINGS";
    private static final String ACCOUNT_SETTINGS_ACCOUNT_KEY = "account";
    private static final int DUAL_PANES_HORIZONTAL_LAYOUT_MIN_WIDTH = 600;

    private SigninUtils() {}

    /**
     * Opens a Settings page to configure settings for a single account.
     * @param activity Activity to use when starting the Activity.
     * @param accountEmail The account email for which the Settings page should be opened.
     * @return Whether or not Android accepted the Intent.
     */
    public static boolean openSettingsForAccount(Activity activity, String accountEmail) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            // ACCOUNT_SETTINGS_ACTION no longer works on Android O+, always open all accounts page.
            return openSettingsForAllAccounts(activity);
        }
        Intent intent = new Intent(ACCOUNT_SETTINGS_ACTION);
        intent.putExtra(
                ACCOUNT_SETTINGS_ACCOUNT_KEY, AccountUtils.createAccountFromName(accountEmail));
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

    /** Returns the accessibility label for the the account picker. */
    public static String getChooseAccountLabel(
            final Context context,
            DisplayableProfileData profileData,
            boolean isCurrentlySelected) {
        if (!isCurrentlySelected) {
            return getAccountLabelForNonSelectedAccount(profileData, context);
        }

        if (profileData.hasDisplayableEmailAddress()) {
            if (TextUtils.isEmpty(profileData.getFullName())) {
                return context.getString(
                        R.string.signin_account_picker_description_with_email,
                        profileData.getAccountEmail());
            }
            return context.getString(
                    R.string.signin_account_picker_description_with_name_and_email,
                    profileData.getFullName(),
                    profileData.getAccountEmail());
        }

        if (TextUtils.isEmpty(profileData.getFullName())) {
            return context.getString(
                    R.string.signin_account_picker_description_without_name_or_email);
        }
        return context.getString(
                R.string.signin_account_picker_description_with_name, profileData.getFullName());
    }

    private static String getAccountLabelForNonSelectedAccount(
            DisplayableProfileData profileData, Context context) {
        if (!profileData.hasDisplayableEmailAddress()) {
            return profileData.getFullName();
        }
        if (TextUtils.isEmpty(profileData.getFullName())) {
            return profileData.getAccountEmail();
        }
        return context.getString(
                R.string.signin_account_label_for_non_selected_account,
                profileData.getFullName(),
                profileData.getAccountEmail());
    }

    /**
     * Returns whether the new sign-in flow should be shown instead of the usual one (sign-in and
     * enable sync for instance) for an sign-in access point eligible to the new flow.
     */
    public static boolean shouldShowNewSigninFlow() {
        return ChromeFeatureList.isEnabled(
                ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS);
    }

    public static View wrapInDialogWhenLargeLayout(View promoContentView) {
        return DialogWhenLargeContentLayout.wrapInDialogWhenLargeLayout(promoContentView);
    }

    /** Returns whether the activity shows on tablet or automotive. */
    public static boolean isTabletOrAuto(Activity activity) {
        return BuildInfo.getInstance().isAutomotive
                || DeviceFormFactor.isNonMultiDisplayContextOnTablet(activity);
    }

    /**
     * Returns whether dual panes horizontal layout can be used on full screen views (e.g. FRE or
     * Upgrade promo sub-views) given the configuration.
     */
    public static boolean shouldShowDualPanesHorizontalLayout(Activity activity) {
        Configuration configuration = activity.getResources().getConfiguration();

        // Since the landscape view has two panes the minimum screenWidth to show it is set to
        // 600dp for phones.
        // Also, the landscape layout is disabled on tablet/auto or large phones since the
        // fullscreen promo is mostly shown as dialog.
        return configuration.orientation == Configuration.ORIENTATION_LANDSCAPE
                && configuration.screenWidthDp >= DUAL_PANES_HORIZONTAL_LAYOUT_MIN_WIDTH
                && !isTabletOrAuto(activity)
                && !DialogWhenLargeContentLayout.shouldShowAsDialog(activity);
    }
}
