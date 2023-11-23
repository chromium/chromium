// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.app.Activity;

import androidx.annotation.IntDef;

import org.chromium.base.Callback;
import org.chromium.base.TimeUtils;
import org.chromium.base.metrics.RecordHistogram;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** ReauthenticationUtils contains static util methods for account reauthentication. */
public class AccountReauthenticationUtils {
    public AccountReauthenticationUtils() {}

    @IntDef({
        RecentAuthenticationResult.HAS_RECENT_AUTHENTICATION,
        RecentAuthenticationResult.NO_RECENT_AUTHENTICATION,
        RecentAuthenticationResult.RECENT_AUTHENTICATION_ERROR
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface RecentAuthenticationResult {
        int HAS_RECENT_AUTHENTICATION = 0;
        int NO_RECENT_AUTHENTICATION = 1;
        int RECENT_AUTHENTICATION_ERROR = 2;
    }

    @IntDef({ConfirmationResult.SUCCESS, ConfirmationResult.REJECTED, ConfirmationResult.ERROR})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ConfirmationResult {
        int SUCCESS = 0;
        int REJECTED = 1;
        int ERROR = 2;
    }

    // These values are persisted to logs. Entries should not be renumbered and numeric values
    // should never be reused.
    @IntDef({
        AccountReauthenticationEvent.STARTED,
        AccountReauthenticationEvent.SUCCESS,
        AccountReauthenticationEvent.SUCCESS_RECENT_AUTHENTICATION,
        AccountReauthenticationEvent.REJECTED,
        AccountReauthenticationEvent.ERROR,
        AccountReauthenticationEvent.COUNT
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface AccountReauthenticationEvent {
        int STARTED = 0;
        int SUCCESS = 1;
        int SUCCESS_RECENT_AUTHENTICATION = 2;
        int REJECTED = 3;
        int ERROR = 4;
        int COUNT = 5;
    }

    static final String ACCOUNT_REAUTHENTICATION_HISTOGRAM = "Signin.AndroidAccountReauth.Event";

    private static void logAccountReauthenticationEvent(@AccountReauthenticationEvent int event) {
        RecordHistogram.recordEnumeratedHistogram(
                ACCOUNT_REAUTHENTICATION_HISTOGRAM, event, AccountReauthenticationEvent.COUNT);
    }

    /**
     * Confirms whether the account has recently been authenticated on this device.
     *
     * @param accountManagerFacade The {@link AccountManagerFacade} to use to confirm
     *         authentication.
     * @param account The {@link Account} to confirm authentication for.
     * @param callback The callback to indicate whether the device had a recent authentication for
     *         the given account, there was no recent authentication, or if confirmation was
     *         interrupted by an exception.
     * @param recentTimeWindowMillis The time window in milliseconds for which a previous
     *         successful authentication can be considered recent.
     */
    public void confirmRecentAuthentication(
            AccountManagerFacade accountManagerFacade,
            Account account,
            @RecentAuthenticationResult Callback<Integer> callback,
            long recentTimeWindowMillis) {
        accountManagerFacade.confirmCredentials(
                account,
                null,
                (response) -> {
                    if (response == null) {
                        callback.onResult(RecentAuthenticationResult.RECENT_AUTHENTICATION_ERROR);
                        return;
                    }
                    if (response.containsKey(AccountManager.KEY_LAST_AUTHENTICATED_TIME)) {
                        Long latestCredentialAuthentication =
                                response.getLong(AccountManager.KEY_LAST_AUTHENTICATED_TIME);
                        if (TimeUtils.currentTimeMillis()
                                <= latestCredentialAuthentication + recentTimeWindowMillis) {
                            callback.onResult(RecentAuthenticationResult.HAS_RECENT_AUTHENTICATION);
                            return;
                        }
                    }
                    callback.onResult(RecentAuthenticationResult.NO_RECENT_AUTHENTICATION);
                });
    }

    /**
     * Confirms that the account has recently been authenticated on this device or launches a
     * re-authentication challenge for the user to confirm their credentials.
     *
     * @param accountManagerFacade The {@link AccountManagerFacade} to use to confirm
     *         authentication.
     * @param account The {@link Account} to confirm authentication for.
     * @param activity The {@link Activity} context to use for launching a new authenticator-defined
     *         sub-Activity to prompt the user to confirm their password.
     * @param callback The callback to indicate whether the device had a recent authentication for
     *         the given account or if the user successfully confirmed their credentials.
     * @param recentTimeWindowMillis The time window in milliseconds for which a previous
     *         successful authentication can be considered recent.
     */
    public void confirmCredentialsOrRecentAuthentication(
            AccountManagerFacade accountManagerFacade,
            Account account,
            Activity activity,
            @ConfirmationResult Callback<Integer> callback,
            long recentTimeWindowMillis) {
        logAccountReauthenticationEvent(AccountReauthenticationEvent.STARTED);
        confirmRecentAuthentication(
                accountManagerFacade,
                account,
                (recentAuthenticationResult) -> {
                    if (RecentAuthenticationResult.HAS_RECENT_AUTHENTICATION
                            == recentAuthenticationResult) {
                        logAccountReauthenticationEvent(
                                AccountReauthenticationEvent.SUCCESS_RECENT_AUTHENTICATION);
                        callback.onResult(ConfirmationResult.SUCCESS);
                        return;
                    }
                    accountManagerFacade.confirmCredentials(
                            account,
                            activity,
                            (response) -> {
                                if (response == null) {
                                    logAccountReauthenticationEvent(
                                            AccountReauthenticationEvent.ERROR);
                                    callback.onResult(ConfirmationResult.ERROR);
                                    return;
                                }
                                if (response.getBoolean(AccountManager.KEY_BOOLEAN_RESULT)) {
                                    logAccountReauthenticationEvent(
                                            AccountReauthenticationEvent.SUCCESS);
                                    callback.onResult(ConfirmationResult.SUCCESS);
                                } else {
                                    logAccountReauthenticationEvent(
                                            AccountReauthenticationEvent.REJECTED);
                                    callback.onResult(ConfirmationResult.REJECTED);
                                }
                            });
                },
                recentTimeWindowMillis);
    }
}
