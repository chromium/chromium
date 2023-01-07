// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

import android.accounts.Account;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Promise;
import org.chromium.components.signin.AccountManagerFacade.ChildAccountStatusListener;
import org.chromium.components.signin.base.CoreAccountInfo;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Locale;
import java.util.regex.Pattern;

/**
 * AccountUtils groups some static util methods for account.
 */
public class AccountUtils {
    private static final Pattern AT_SYMBOL = Pattern.compile("@");
    private static final String GMAIL_COM = "gmail.com";
    private static final String GOOGLEMAIL_COM = "googlemail.com";

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public static final String GOOGLE_ACCOUNT_TYPE = "com.google";

    private AccountUtils() {}

    /**
     * Creates an Account object for the given name.
     */
    public static Account createAccountFromName(String name) {
        return new Account(name, GOOGLE_ACCOUNT_TYPE);
    }

    /**
     * Converts a list of accounts to a list of account names.
     */
    public static List<String> toAccountNames(final List<Account> accounts) {
        List<String> accountNames = new ArrayList<>();
        for (Account account : accounts) {
            accountNames.add(account.name);
        }
        return accountNames;
    }

    /**
     * Converts a list of {@link CoreAccountInfo} to a list of {@link Account}.
     */
    public static List<Account> toAndroidAccounts(final List<CoreAccountInfo> accounts) {
        List<Account> androidAccounts = new ArrayList<>();
        for (CoreAccountInfo account : accounts) {
            androidAccounts.add(createAccountFromName(account.getEmail()));
        }
        return androidAccounts;
    }

    /**
     * Finds the first account of the account list whose canonical name equal the given
     * accountName's canonical name; null if account does not exist.
     */
    public static @Nullable Account findAccountByName(
            final List<Account> accounts, String accountName) {
        String canonicalName = AccountUtils.canonicalizeName(accountName);
        for (Account account : accounts) {
            if (AccountUtils.canonicalizeName(account.name).equals(canonicalName)) {
                return account;
            }
        }
        return null;
    }

    /**
     * Gets the cached list of accounts from the given {@link Promise}.
     * If the cache is not yet populated, return an empty list.
     */
    public static List<Account> getAccountsIfFulfilledOrEmpty(Promise<List<Account>> promise) {
        return promise.isFulfilled() ? promise.getResult() : Collections.emptyList();
    }

    /**
     * Gets the cached default accounts from the given {@link Promise}.
     * If the cache is not yet populated or no accounts exist, return null.
     */
    public static @Nullable Account getDefaultAccountIfFulfilled(Promise<List<Account>> promise) {
        final List<Account> accounts = getAccountsIfFulfilledOrEmpty(promise);
        return accounts.isEmpty() ? null : accounts.get(0);
    }

    /**
     * Checks the child account status on device based on the list of (zero or more) provided
     * accounts.
     *
     * If there are no child accounts on the device, the listener will be invoked with
     * isChild = false. If there is a child account on device, the listener
     * will be called with that account and isChild = true. Note that it is not currently possible
     * to have more than one child account on device.
     *
     * It should be safe to invoke this method before the native library is initialized.
     *
     * @param accountManagerFacade The singleton instance of {@link AccountManagerFacade}.
     * @param accounts The list of accounts on device.
     * @param listener The listener is called when the status of the account
     *                 (whether it is a child one) is ready.
     */
    public static void checkChildAccountStatus(@NonNull AccountManagerFacade accountManagerFacade,
            @NonNull List<Account> accounts, @NonNull ChildAccountStatusListener listener) {
        if (accounts.size() >= 1) {
            // If a child account is present then there can be only one, and it must be the first
            // account on the device.
            accountManagerFacade.checkChildAccountStatus(accounts.get(0), listener);
        } else {
            listener.onStatusReady(false, null);
        }
    }

    /**
     * Canonicalizes the account name.
     */
    static String canonicalizeName(String name) {
        String[] parts = AT_SYMBOL.split(name);
        if (parts.length != 2) return name;

        if (GOOGLEMAIL_COM.equalsIgnoreCase(parts[1])) {
            parts[1] = GMAIL_COM;
        }
        if (GMAIL_COM.equalsIgnoreCase(parts[1])) {
            parts[0] = parts[0].replace(".", "");
        }
        return (parts[0] + "@" + parts[1]).toLowerCase(Locale.US);
    }
}
