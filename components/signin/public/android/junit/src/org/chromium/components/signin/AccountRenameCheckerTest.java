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

import org.chromium.base.task.test.CustomShadowAsyncTask;
import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.atomic.AtomicReference;

/**
 * JUnit tests of the class {@link AccountRenameChecker}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {AccountRenameCheckerTest.ShadowGoogleAuthUtil.class,
                CustomShadowAsyncTask.class})
public class AccountRenameCheckerTest {
    @Implements(GoogleAuthUtil.class)
    static final class ShadowGoogleAuthUtil {
        private static final Map<String, List<AccountChangeEvent>> sEvents = new HashMap<>();

        @Implementation
        public static List<AccountChangeEvent> getAccountChangeEvents(
                Context context, int eventIndex, String accountEmail) {
            return sEvents.getOrDefault(accountEmail, Collections.emptyList());
        }

        static void insertRenameEvent(String from, String to) {
            addEvent(from,
                    new AccountChangeEvent(
                            0L, from, GoogleAuthUtil.CHANGE_TYPE_ACCOUNT_RENAMED_TO, 0, to));
        }

        static void addEvent(String email, AccountChangeEvent event) {
            final List<AccountChangeEvent> events = sEvents.getOrDefault(email, new ArrayList<>());
            events.add(event);
            sEvents.put(email, events);
        }

        static void clearAllEvents() {
            sEvents.clear();
        }
    }

    private final AccountRenameChecker mChecker = AccountRenameChecker.get();

    @After
    public void tearDown() {
        ShadowGoogleAuthUtil.clearAllEvents();
    }

    @Test
    public void newNameIsValidWhenTheRenamedAccountIsPresent() {
        ShadowGoogleAuthUtil.insertRenameEvent("A", "B");
        final AtomicReference<String> newAccountName = new AtomicReference<>();

        mChecker.getNewNameOfRenamedAccountAsync("A", getAccounts("B")).then(newAccountName::set);

        Assert.assertEquals("B", newAccountName.get());
    }

    @Test
    public void newNameIsValidWhenOldAccountIsRemovedAndThenRenamed() {
        ShadowGoogleAuthUtil.addEvent("A",
                new AccountChangeEvent(0L, "A", GoogleAuthUtil.CHANGE_TYPE_ACCOUNT_REMOVED, 0, ""));
        ShadowGoogleAuthUtil.insertRenameEvent("A", "B");
        final AtomicReference<String> newAccountName = new AtomicReference<>();

        mChecker.getNewNameOfRenamedAccountAsync("A", getAccounts("B")).then(newAccountName::set);

        Assert.assertEquals("B", newAccountName.get());
    }

    @Test
    public void newNameIsNullWhenTheOldAccountIsNotRenamed() {
        ShadowGoogleAuthUtil.insertRenameEvent("B", "C");
        final AtomicReference<String> newAccountName = new AtomicReference<>();

        mChecker.getNewNameOfRenamedAccountAsync("A", getAccounts("D")).then(newAccountName::set);

        Assert.assertNull(newAccountName.get());
    }

    @Test
    public void newNameIsNullWhenTheRenamedAccountIsNotPresent() {
        ShadowGoogleAuthUtil.insertRenameEvent("B", "C");
        final AtomicReference<String> newAccountName = new AtomicReference<>();

        mChecker.getNewNameOfRenamedAccountAsync("B", getAccounts("D")).then(newAccountName::set);

        Assert.assertNull(newAccountName.get());
    }

    @Test
    public void newNameIsValidWhenTheOldAccountIsRenamedTwice() {
        ShadowGoogleAuthUtil.insertRenameEvent("A", "B");
        ShadowGoogleAuthUtil.insertRenameEvent("B", "C");
        final AtomicReference<String> newAccountName = new AtomicReference<>();

        mChecker.getNewNameOfRenamedAccountAsync("A", getAccounts("C")).then(newAccountName::set);

        Assert.assertEquals("C", newAccountName.get());
    }

    @Test
    public void newNameIsValidWhenTheOldAccountIsRenamedMultipleTimes() {
        // A -> B -> C
        ShadowGoogleAuthUtil.insertRenameEvent("Z", "Y"); // Unrelated.
        ShadowGoogleAuthUtil.insertRenameEvent("A", "B");
        ShadowGoogleAuthUtil.insertRenameEvent("Y", "X"); // Unrelated.
        ShadowGoogleAuthUtil.insertRenameEvent("B", "C");
        ShadowGoogleAuthUtil.insertRenameEvent("C", "D");
        final AtomicReference<String> newAccountName = new AtomicReference<>();

        mChecker.getNewNameOfRenamedAccountAsync("A", getAccounts("D")).then(newAccountName::set);

        Assert.assertEquals("D", newAccountName.get());
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
        final AtomicReference<String> newAccountName = new AtomicReference<>();

        mChecker.getNewNameOfRenamedAccountAsync("A", getAccounts("D", "X"))
                .then(newAccountName::set);

        Assert.assertEquals("D", newAccountName.get());
    }

    private List<Account> getAccounts(String... names) {
        final List<Account> accounts = new ArrayList<>();
        for (String name : names) {
            accounts.add(AccountUtils.createAccountFromName(name));
        }
        return accounts;
    }
}
