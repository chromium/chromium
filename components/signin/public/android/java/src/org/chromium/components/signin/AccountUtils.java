// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

import android.accounts.Account;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Promise;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.signin.AccountManagerFacade.ChildAccountStatusListener;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.GaiaId;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Locale;
import java.util.regex.Pattern;

/** AccountUtils groups some static util methods for account. */
@NullMarked
public class AccountUtils {
    private static final Pattern AT_SYMBOL = Pattern.compile("@");
    private static final String GMAIL_COM = "gmail.com";
    private static final String GOOGLEMAIL_COM = "googlemail.com";

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public static final String GOOGLE_ACCOUNT_TYPE = "com.google";

    private AccountUtils() {}

    /**
     * Creates an Account object for the given {@param email}. Only used in places where we need to
     * talk to Android which is very rare.
     */
    public static Account createAccountFromEmail(String email) {
        return new Account(email, GOOGLE_ACCOUNT_TYPE);
    }

    /** Converts a list of {@link AccountInfo}s to a list of account emails. */
    public static List<String> toAccountEmails(final List<AccountInfo> accounts) {
        int size = accounts.size();
        String[] emails = new String[size];
        for (int i = 0; i < size; ++i) {
            emails[i] = accounts.get(i).getEmail();
        }
        return Arrays.asList(emails);
    }

    /**
     * Finds the first {@link AccountInfo} among `accounts` whose canonical email is equal to
     * `accountEmail`; `null` if there is no match.
     */
    public static @Nullable AccountInfo findAccountByEmail(
            List<AccountInfo> accounts, String accountEmail) {
        String canonicalEmail = AccountUtils.canonicalizeEmail(accountEmail);
        for (AccountInfo account : accounts) {
            if (AccountUtils.canonicalizeEmail(account.getEmail()).equals(canonicalEmail)) {
                return account;
            }
        }
        return null;
    }

    /**
     * Finds the first {@link AccountInfo} among `accounts` whose Gaia ID is equal to
     * `accountGaiaId`; null if there is no match.
     */
    public static @Nullable AccountInfo findAccountByGaiaId(
            final List<AccountInfo> accounts, GaiaId accountGaiaId) {
        for (AccountInfo account : accounts) {
            if (account.getGaiaId().equals(accountGaiaId)) {
                return account;
            }
        }
        return null;
    }

    /**
     * Gets the cached list of {@link AccountInfo} from the given `promise`. If the cache is not yet
     * populated, return an empty list.
     */
    public static List<AccountInfo> getAccountsIfFulfilledOrEmpty(
            Promise<List<AccountInfo>> promise) {
        return promise.isFulfilled() ? promise.getResult() : Collections.emptyList();
    }

    /**
     * Gets the cached default {@link AccountInfo} from the given {@link Promise}. If the cache is
     * not yet populated or no accounts exist, return null.
     */
    public static @Nullable AccountInfo getDefaultAccountIfFulfilled(
            Promise<List<AccountInfo>> promise) {
        final List<AccountInfo> accounts = getAccountsIfFulfilledOrEmpty(promise);
        return accounts.isEmpty() ? null : accounts.get(0);
    }

    /**
     * Checks the child account status on device based on the list of (zero or more) provided
     * `accounts`.
     *
     * <p>If there are no child account on the device, the listener will be invoked with isChild =
     * false. If there is a child account on device, the listener will be called with that account
     * and isChild = true. Note that it is not currently possible to have more than one child
     * account on device.
     *
     * <p>It should be safe to invoke this method before the native library is initialized.
     *
     * @param accountManagerFacade The singleton instance of {@link AccountManagerFacade}.
     * @param accounts The list of {@link AccountInfo} on device.
     * @param listener The listener is called when the status of the account (whether it is a child
     *     one) is ready.
     */
    public static void checkChildAccountStatus(
            AccountManagerFacade accountManagerFacade,
            List<AccountInfo> accounts,
            ChildAccountStatusListener listener) {
        if (!accounts.isEmpty()) {
            // If a child account is present then there can be only one, and it must be the first
            // account on the device.
            accountManagerFacade.checkChildAccountStatus(accounts.get(0), listener);
        } else {
            listener.onStatusReady(false, null);
        }
    }

    /**
     * Checks the parental control subjectivity of the accounts on the device based on the list of
     * (zero or more) provided `accounts`.
     *
     * <p>If there are no account subject to parental controls on the device, the listener will be
     * invoked with isChild = false. If there is an account subject to parental controls on device,
     * the listener will be called with that account and isChild = true. Note that it is not
     * currently possible to have more than one account subject to parental controls on device.
     *
     * <p>It should be safe to invoke this method before the native library is initialized.
     *
     * @param accountManagerFacade The singleton instance of {@link AccountManagerFacade}.
     * @param accounts The list of {@link AccountInfo} on device.
     * @param listener The listener is called when the status of the account (whether it is subject
     *     to parental controls) is ready.
     */
    public static void checkIsSubjectToParentalControls(
            AccountManagerFacade accountManagerFacade,
            List<AccountInfo> accounts,
            ChildAccountStatusListener listener) {
        if (!accounts.isEmpty()) {
            // If an account subject to parental controls is present then there can be only one, and
            // it must be the first
            // account on the device.
            accountManagerFacade.checkIsSubjectToParentalControls(accounts.get(0), listener);
        } else {
            listener.onStatusReady(false, null);
        }
    }

    /** Canonicalizes the account email. */
    static String canonicalizeEmail(String email) {
        String[] parts = AT_SYMBOL.split(email);
        if (parts.length != 2) return email;

        if (GOOGLEMAIL_COM.equalsIgnoreCase(parts[1])) {
            parts[1] = GMAIL_COM;
        }
        if (GMAIL_COM.equalsIgnoreCase(parts[1])) {
            parts[0] = parts[0].replace(".", "");
        }
        return (parts[0] + "@" + parts[1]).toLowerCase(Locale.US);
    }
}
