// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

import android.accounts.Account;
import android.content.Context;

import com.google.android.gms.auth.AccountChangeEvent;
import com.google.android.gms.auth.GoogleAuthUtil;

import org.junit.After;
import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * JUnit tests of the class {@link AccountRenameChecker}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {AccountRenameCheckerTest.ShadowGoogleAuthUtil.class})
public class AccountRenameCheckerTest {
    @Implements(GoogleAuthUtil.class)
    static final class ShadowGoogleAuthUtil {
        private static final Map<String, String> sEvents = new HashMap<>();

        @Implementation
        public static List<AccountChangeEvent> getAccountChangeEvents(
                Context context, int eventIndex, String accountEmail) {
            if (sEvents.containsKey(accountEmail)) {
                final AccountChangeEvent event = new AccountChangeEvent(0L, accountEmail,
                        GoogleAuthUtil.CHANGE_TYPE_ACCOUNT_RENAMED_TO, 0,
                        sEvents.get(accountEmail));
                return List.of(event);
            }
            return Collections.emptyList();
        }

        static void insertRenameEvent(String from, String to) {
            sEvents.put(from, to);
        }

        static void clearAllEvents() {
            sEvents.clear();
        }
    }

    private final AccountRenameChecker mChecker = new AccountRenameChecker();

    @After
    public void tearDown() {
        ShadowGoogleAuthUtil.clearAllEvents();
    }

    @Test
    public void newNameIsValidWhenTheRenamedAccountIsPresent() {
        ShadowGoogleAuthUtil.insertRenameEvent("A", "B");

        Assert.assertEquals("B", mChecker.getNewNameOfRenamedAccount("A", getAccounts("B")));
    }

    @Test
    public void newNameIsNullWhenTheOldAccountIsNotRenamed() {
        ShadowGoogleAuthUtil.insertRenameEvent("B", "C");

        Assert.assertNull(mChecker.getNewNameOfRenamedAccount("A", getAccounts("D")));
    }

    @Test
    public void newNameIsNullWhenTheRenamedAccountIsNotPresent() {
        ShadowGoogleAuthUtil.insertRenameEvent("B", "C");

        Assert.assertNull(mChecker.getNewNameOfRenamedAccount("B", getAccounts("D")));
    }

    @Test
    public void newNameIsValidWhenTheOldAccountIsRenamedTwice() {
        ShadowGoogleAuthUtil.insertRenameEvent("A", "B");
        ShadowGoogleAuthUtil.insertRenameEvent("B", "C");

        Assert.assertEquals("C", mChecker.getNewNameOfRenamedAccount("A", getAccounts("C")));
    }

    @Test
    public void newNameIsValidWhenTheOldAccountIsRenamedMultipleTimes() {
        // A -> B -> C
        ShadowGoogleAuthUtil.insertRenameEvent("Z", "Y"); // Unrelated.
        ShadowGoogleAuthUtil.insertRenameEvent("A", "B");
        ShadowGoogleAuthUtil.insertRenameEvent("Y", "X"); // Unrelated.
        ShadowGoogleAuthUtil.insertRenameEvent("B", "C");
        ShadowGoogleAuthUtil.insertRenameEvent("C", "D");

        Assert.assertEquals("D", mChecker.getNewNameOfRenamedAccount("A", getAccounts("D")));
    }

    @Test
    public void newNameIsValidWhenTheOldAccountIsRenamedInCycle() {
        // A -> B -> C -> D -> A
        ShadowGoogleAuthUtil.insertRenameEvent("Z", "Y"); // Unrelated.
        ShadowGoogleAuthUtil.insertRenameEvent("A", "B");
        ShadowGoogleAuthUtil.insertRenameEvent("Y", "X"); // Unrelated.
        ShadowGoogleAuthUtil.insertRenameEvent("B", "C");
        ShadowGoogleAuthUtil.insertRenameEvent("C", "D");
        ShadowGoogleAuthUtil.insertRenameEvent("D", "A"); // Looped.

        Assert.assertEquals("D", mChecker.getNewNameOfRenamedAccount("A", getAccounts("D", "X")));
    }

    private List<Account> getAccounts(String... names) {
        final List<Account> accounts = new ArrayList<>();
        for (String name : names) {
            accounts.add(AccountUtils.createAccountFromName(name));
        }
        return accounts;
    }
}
