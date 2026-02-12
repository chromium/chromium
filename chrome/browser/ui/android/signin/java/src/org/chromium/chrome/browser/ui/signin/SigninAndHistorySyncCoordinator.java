// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.accounts.AccountManager;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;

import org.chromium.base.IntentUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils.State;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncHelper;
import org.chromium.components.browser_ui.settings.ManagedPreferencesUtils;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler.BackPressResult;
import org.chromium.components.signin.SigninFeatureMap;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.widget.Toast;

import java.util.Objects;

/**
 * Interface for coordinators responsible of showing the correct sub-component of the sign-in and
 * history opt-in flow.
 */
@NullMarked
public abstract class SigninAndHistorySyncCoordinator {

    /** Indicates the sign-in flow completion status. */
    public static class Result {
        /**
         * Whether the sign-in operation occurred during this specific execution of the flow. Should
         * be False if the user was already signed in before the flow started. Note, if the user
         * explicitly accepts the sign-in CTA and the history sync consent is required, then
         * declining history sync invalidates the entire sign-in.
         */
        public final boolean hasSignedIn;

        /**
         * The user successfully completed the history sync enablement step during the flow. Note,
         * it possible for an already signed-in user to not be shown the sign-in CTA and only the
         * history sync consent dialog.
         */
        public final boolean hasOptedInHistorySync;

        public Result(boolean hasSignedIn, boolean hasOptedInHistorySync) {
            this.hasSignedIn = hasSignedIn;
            this.hasOptedInHistorySync = hasOptedInHistorySync;
        }

        /** Default non-completion state, user canceled the sign-in flow, or an error occurred. */
        public static Result aborted() {
            return new Result(false, false);
        }

        @Override
        public int hashCode() {
            return Objects.hash(hasSignedIn, hasOptedInHistorySync);
        }

        @Override
        public boolean equals(@Nullable Object obj) {
            if (obj instanceof Result result) {
                return hasSignedIn == result.hasSignedIn
                        && hasOptedInHistorySync == result.hasOptedInHistorySync;
            }
            return false;
        }

        @Override
        public String toString() {
            return "Result{ hasSignedIn: "
                    + hasSignedIn
                    + ", hasOptedInHistorySync: "
                    + hasOptedInHistorySync
                    + '}';
        }
    }

    /** Cleans up the coordinator after it is finished being used. */
    public abstract void destroy();

    /** Called when the configuration of the embedder activity changes. */
    public abstract void onConfigurationChange();

    /** Called when a backpress occurs in the embedder activity. */
    @BackPressResult
    public abstract int handleBackPress();

    /**
     * Called when an Google Play Services "add account" flow started at the activity level has
     * finished with a result.
     */
    public final void onAddAccountResult(int resultCode, @Nullable Intent data) {
        final String accountEmail =
                data == null
                        ? null
                        : IntentUtils.safeGetStringExtra(data, AccountManager.KEY_ACCOUNT_NAME);

        if (resultCode != Activity.RESULT_OK || accountEmail == null) {
            // Record NULL_ACCOUNT_NAME if the add account activity successfully returns but
            // contains a null account name.
            if (resultCode == Activity.RESULT_OK && accountEmail == null) {
                SigninMetricsUtils.logAddAccountStateHistogram(State.NULL_ACCOUNT_NAME);
            } else {
                SigninMetricsUtils.logAddAccountStateHistogram(State.CANCELLED);
            }
            onAddAccountCanceled();
            return;
        }

        SigninMetricsUtils.logAddAccountStateHistogram(State.SUCCEEDED);
        onAccountAdded(accountEmail);
    }

    /**
     * Called by {@link onAddAccountResult} when an Google Play Services "add account" flow started
     * at the activity level has finished without being completed.
     */
    protected abstract void onAddAccountCanceled();

    /**
     * Called by {@link onAddAccountResult} when an Google Play Services "add account" flow started
     * at the activity level has finished after being completed.
     *
     * @param accountEmail the email of the added account.
     */
    protected abstract void onAccountAdded(String accountEmail);

    /**
     * Whether the sign-in ui will show in the sign-in flow if the latter is launched.
     *
     * <p>The sign-in UI can be skipped if the user is already signed-in, for instance.
     *
     * @param profile The current profile.
     */
    public static boolean willShowSigninUi(Profile profile) {
        SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(profile);
        assumeNonNull(signinManager);
        return signinManager.isSigninAllowed();
    }

    /**
     * Whether the history sync opt-in ui will show in the sign-in flow if the latter is launched.
     *
     * <p>The history sync opt-in can be skipped if the user is already opted-in, or if sync or the
     * history sync data types are managed, for instance.
     *
     * @param profile The current profile.
     * @param historyOptInMode Whether the history opt-in should be always, optionally or never
     *     shown.
     */
    public static boolean willShowHistorySyncUi(
            Profile profile, @HistorySyncConfig.OptInMode int historyOptInMode) {
        IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(profile);
        assumeNonNull(identityManager);
        if (!willShowSigninUi(profile) && !identityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)) {
            // Signin is suppressed because of something other than the user being signed in. Since
            // the user cannot sign in, we should not show history sync either.
            return false;
        }
        return shouldShowHistorySync(profile, historyOptInMode);
    }

    public static boolean shouldShowHistorySync(
            Profile profile, @HistorySyncConfig.OptInMode int historyOptInMode) {
        HistorySyncHelper historySyncHelper = HistorySyncHelper.getForProfile(profile);
        boolean forceHistoryOptInScreen =
                SigninFeatureMap.isEnabled(SigninFeatures.FORCE_HISTORY_OPT_IN_SCREEN);
        return switch (historyOptInMode) {
            case HistorySyncConfig.OptInMode.NONE -> false;
            case HistorySyncConfig.OptInMode.OPTIONAL ->
                    historySyncHelper.shouldDisplayHistorySync()
                            && (forceHistoryOptInScreen || !historySyncHelper.isDeclinedOften());
            case HistorySyncConfig.OptInMode.REQUIRED ->
                    historySyncHelper.shouldDisplayHistorySync();
            default ->
                    throw new IllegalArgumentException(
                            "Unexpected value for historyOptInMode :" + historyOptInMode);
        };
    }

    /**
     * Checks whether the sign-in and history sync flow can be started (at least the sign-in UI or
     * the history sync UI will be shown if the flow starts) according to the given configuration
     * and other parameters. It shows an error toast if the flow can't start.
     *
     * @return true if the flow can start, false otherwise.
     */
    public static boolean canStartSigninAndHistorySyncOrShowError(
            Context context,
            Profile profile,
            @HistorySyncConfig.OptInMode int historyOptInMode,
            @SigninAccessPoint int accessPoint) {
        if (SigninAndHistorySyncCoordinator.willShowSigninUi(profile)
                || SigninAndHistorySyncCoordinator.willShowHistorySyncUi(
                        profile, historyOptInMode)) {
            return true;
        }
        // TODO(crbug.com/354912290): Update the UI related to sign-in errors.
        if (UserPrefs.get(profile).isManagedPreference(Pref.SIGNIN_ALLOWED)) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Signin.SigninDisabledNotificationShown",
                    accessPoint,
                    SigninAccessPoint.MAX_VALUE);
            ManagedPreferencesUtils.showManagedByAdministratorToast(context);
        } else {
            Toast.makeText(
                            context,
                            context.getString(
                                    R.string.signin_account_picker_bottom_sheet_error_title),
                            Toast.LENGTH_LONG)
                    .show();
        }
        return false;
    }
}
