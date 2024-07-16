// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.SystemClock;
import android.provider.Settings;
import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.IntentUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninManager.SignInCallback;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.SigninFeatureMap;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Helper functions for sign-in and accounts. */
public final class SigninUtils {
    private static final String ACCOUNT_SETTINGS_ACTION = "android.settings.ACCOUNT_SYNC_SETTINGS";
    private static final String ACCOUNT_SETTINGS_ACCOUNT_KEY = "account";
    private static final String UNMANAGED_SIGNIN_DURATION_NAME =
            "Signin.Android.FREUnmanagedAccountSigninDuration";
    private static final String FRE_SIGNIN_EVENTS_NAME = "Signin.Android.FRESigninEvents";

    @IntDef({
        FRESigninEvents.CHECKING_MANAGED_STATUS,
        FRESigninEvents.SIGNING_IN_UNMANAGED,
        FRESigninEvents.SIGNIN_COMPLETE_UNMANAGED,
        FRESigninEvents.ACCEPTING_MANAGEMENT,
        FRESigninEvents.SIGNING_IN_MANAGED,
        FRESigninEvents.SIGNIN_COMPLETE_MANAGED,
        FRESigninEvents.SIGNIN_ABORTED_UNMANAGED,
        FRESigninEvents.SIGNIN_ABORTED_MANAGED,
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface FRESigninEvents {
        int CHECKING_MANAGED_STATUS = 0;
        int SIGNING_IN_UNMANAGED = 1;
        int SIGNIN_COMPLETE_UNMANAGED = 2;
        int ACCEPTING_MANAGEMENT = 3;
        int SIGNING_IN_MANAGED = 4;
        int SIGNIN_COMPLETE_MANAGED = 5;
        int SIGNIN_ABORTED_UNMANAGED = 6;
        int SIGNIN_ABORTED_MANAGED = 7;
        int NUM_ENTRIES = 8;
    }

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

    private static class WrappedSigninCallback implements SigninManager.SignInCallback {
        private SigninManager.SignInCallback mWrappedCallback;

        public WrappedSigninCallback(SigninManager.SignInCallback callback) {
            mWrappedCallback = callback;
        }

        @Override
        public void onSignInComplete() {
            if (mWrappedCallback != null) mWrappedCallback.onSignInComplete();
        }

        @Override
        public void onPrefsCommitted() {
            if (mWrappedCallback != null) mWrappedCallback.onPrefsCommitted();
        }

        @Override
        public void onSignInAborted() {
            if (mWrappedCallback != null) mWrappedCallback.onSignInAborted();
        }
    }

    /** Performs signin after confirming account management with the user, if necessary. */
    public static void checkAccountManagementAndSignIn(
            CoreAccountInfo coreAccountInfo,
            SigninManager signinManager,
            @SigninAccessPoint int accessPoint,
            @Nullable SignInCallback callback,
            Context context,
            ModalDialogManager modalDialogManager) {
        long startTimeMillis = SystemClock.uptimeMillis();
        if (!SigninFeatureMap.isEnabled(SigninFeatures.ENTERPRISE_POLICY_ON_SIGNIN)
                || signinManager.getUserAcceptedAccountManagement()) {
            SignInCallback wrappedCallback =
                    new WrappedSigninCallback(callback) {
                        @Override
                        public void onSignInComplete() {
                            RecordHistogram.recordMediumTimesHistogram(
                                    UNMANAGED_SIGNIN_DURATION_NAME,
                                    SystemClock.uptimeMillis() - startTimeMillis);
                            recordFREEvent(FRESigninEvents.SIGNIN_COMPLETE_UNMANAGED);
                            super.onSignInComplete();
                        }

                        @Override
                        public void onSignInAborted() {
                            recordFREEvent(FRESigninEvents.SIGNIN_ABORTED_UNMANAGED);
                            super.onSignInAborted();
                        }
                    };
            recordFREEvent(FRESigninEvents.SIGNING_IN_UNMANAGED);
            signinManager.signin(coreAccountInfo, accessPoint, wrappedCallback);
            return;
        }
        recordFREEvent(FRESigninEvents.CHECKING_MANAGED_STATUS);
        signinManager.isAccountManaged(
                coreAccountInfo,
                (Boolean isAccountManaged) -> {
                    onIsAccountManaged(
                            isAccountManaged,
                            coreAccountInfo,
                            signinManager,
                            accessPoint,
                            callback,
                            context,
                            modalDialogManager,
                            startTimeMillis);
                });
    }

    private static void onIsAccountManaged(
            Boolean isAccountManaged,
            CoreAccountInfo coreAccountInfo,
            SigninManager signinManager,
            @SigninAccessPoint int accessPoint,
            @Nullable SignInCallback callback,
            Context context,
            ModalDialogManager modalDialogManager,
            long startTimeMillis) {
        if (!isAccountManaged) {
            SignInCallback wrappedCallback =
                    new WrappedSigninCallback(callback) {
                        @Override
                        public void onSignInComplete() {
                            RecordHistogram.recordMediumTimesHistogram(
                                    UNMANAGED_SIGNIN_DURATION_NAME,
                                    SystemClock.uptimeMillis() - startTimeMillis);
                            recordFREEvent(FRESigninEvents.SIGNIN_COMPLETE_UNMANAGED);
                            super.onSignInComplete();
                        }

                        @Override
                        public void onSignInAborted() {
                            recordFREEvent(FRESigninEvents.SIGNIN_ABORTED_UNMANAGED);
                            super.onSignInAborted();
                        }
                    };
            recordFREEvent(FRESigninEvents.SIGNING_IN_UNMANAGED);
            signinManager.signin(coreAccountInfo, accessPoint, wrappedCallback);
            return;
        }

        SignInCallback wrappedCallback =
                new WrappedSigninCallback(callback) {
                    @Override
                    public void onSignInAborted() {
                        recordFREEvent(FRESigninEvents.SIGNIN_ABORTED_MANAGED);
                        // If signin is aborted, we need to clear the account management acceptance.
                        signinManager.setUserAcceptedAccountManagement(false);
                        super.onSignInAborted();
                    }

                    @Override
                    public void onSignInComplete() {
                        recordFREEvent(FRESigninEvents.SIGNIN_COMPLETE_MANAGED);
                        super.onSignInComplete();
                    }
                };

        ConfirmManagedSyncDataDialogCoordinator.Listener listener =
                new ConfirmManagedSyncDataDialogCoordinator.Listener() {
                    @Override
                    public void onConfirm() {
                        signinManager.setUserAcceptedAccountManagement(true);
                        recordFREEvent(FRESigninEvents.SIGNING_IN_MANAGED);
                        signinManager.signin(coreAccountInfo, accessPoint, wrappedCallback);
                    }

                    @Override
                    public void onCancel() {
                        if (callback != null) callback.onSignInAborted();
                    }
                };

        recordFREEvent(FRESigninEvents.ACCEPTING_MANAGEMENT);
        new ConfirmManagedSyncDataDialogCoordinator(
                context,
                modalDialogManager,
                listener,
                signinManager.extractDomainName(coreAccountInfo.getEmail()));
    }

    private static void recordFREEvent(@FRESigninEvents int event) {
        RecordHistogram.recordEnumeratedHistogram(
                FRE_SIGNIN_EVENTS_NAME, event, FRESigninEvents.NUM_ENTRIES);
    }

    /**
     * Returns whether the new sign-in flow should be shown instead of the usual one (sign-in and
     * enable sync for instance) for an sign-in access point eligible to the new flow.
     */
    public static boolean shouldShowNewSigninFlow() {
        return ChromeFeatureList.isEnabled(
                ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS);
    }
}
