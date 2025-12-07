// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.test.util;

import android.content.Context;
import android.content.SharedPreferences;

import androidx.annotation.MainThread;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.components.signin.base.AccountInfo;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Manages persistence of fake accounts to SharedPreferences for testing. */
final class SharedPrefsAccountStorage {
    private static final String FAKE_ACCOUNTS_PREF = "FakeAccountManagerFacade.ACCOUNTS";
    private static final String ACCOUNTS_KEY = "accounts";

    private SharedPrefsAccountStorage() {}

    @MainThread
    static List<AccountInfo> loadAccounts() {
        ThreadUtils.checkUiThread();
        SharedPreferences prefs =
                ContextUtils.getApplicationContext()
                        .getSharedPreferences(FAKE_ACCOUNTS_PREF, Context.MODE_PRIVATE);
        Set<String> serializedAccounts = prefs.getStringSet(ACCOUNTS_KEY, null);
        if (serializedAccounts == null) {
            return new ArrayList<>();
        }

        List<AccountInfo> accounts = new ArrayList<>();
        for (String serializedAccount : serializedAccounts) {
            AccountInfo accountInfo = AccountInfoSerializer.fromJsonString(serializedAccount);
            if (accountInfo != null) {
                accounts.add(accountInfo);
            } else {
                throw new IllegalStateException(
                        "Error deserializing account: " + serializedAccount);
            }
        }
        return accounts;
    }

    @MainThread
    static void saveAccounts(List<AccountInfo> accounts) {
        ThreadUtils.checkUiThread();
        Set<String> serializedAccounts = new HashSet<>();
        for (AccountInfo accountInfo : accounts) {
            String serializedAccountInfo = AccountInfoSerializer.toJsonString(accountInfo);
            if (serializedAccountInfo != null) {
                serializedAccounts.add(serializedAccountInfo);
            } else {
                throw new IllegalStateException(
                        "Error serializing account: " + accountInfo.getEmail());
            }
        }

        SharedPreferences prefs =
                ContextUtils.getApplicationContext()
                        .getSharedPreferences(FAKE_ACCOUNTS_PREF, Context.MODE_PRIVATE);
        prefs.edit().putStringSet(ACCOUNTS_KEY, serializedAccounts).commit();
    }
}
