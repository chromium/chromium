// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

import android.content.Context;

import androidx.annotation.Nullable;

import com.google.android.gms.auth.AccountChangeEvent;
import com.google.android.gms.auth.GoogleAuthUtil;

import org.junit.After;
import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.task.test.CustomShadowAsyncTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.signin.base.CoreAccountInfo;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.atomic.AtomicReference;

/** JUnit tests of the class {@link AccountRenameChecker}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        shadows = {
            AccountRenameCheckerTest.ShadowGoogleAuthUtil.class,
            CustomShadowAsyncTask.class
        })
@LooperMode(LooperMode.Mode.LEGACY)
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
            addEvent(
                    from,
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

        Assert.assertEquals("B", getNewNameOfRenamedAccount("A", List.of("B")));
    }

    @Test
    public void newNameIsValidWhenOldAccountIsRemovedAndThenRenamed() {
        ShadowGoogleAuthUtil.addEvent(
                "A",
                new AccountChangeEvent(0L, "A", GoogleAuthUtil.CHANGE_TYPE_ACCOUNT_REMOVED, 0, ""));
        ShadowGoogleAuthUtil.insertRenameEvent("A", "B");

        Assert.assertEquals("B", getNewNameOfRenamedAccount("A", List.of("B")));
    }

    @Test
    public void newNameIsNullWhenTheOldAccountIsNotRenamed() {
        ShadowGoogleAuthUtil.insertRenameEvent("B", "C");

        Assert.assertNull(getNewNameOfRenamedAccount("A", List.of("D")));
    }

    @Test
    public void newNameIsNullWhenTheRenamedAccountIsNotPresent() {
        ShadowGoogleAuthUtil.insertRenameEvent("B", "C");

        Assert.assertNull(getNewNameOfRenamedAccount("B", List.of("D")));
    }

    @Test
    public void newNameIsValidWhenTheOldAccountIsRenamedTwice() {
        ShadowGoogleAuthUtil.insertRenameEvent("A", "B");
        ShadowGoogleAuthUtil.insertRenameEvent("B", "C");

        Assert.assertEquals("C", getNewNameOfRenamedAccount("A", List.of("C")));
    }

    @Test
    public void newNameIsValidWhenTheOldAccountIsRenamedMultipleTimes() {
        // A -> B -> C
        ShadowGoogleAuthUtil.insertRenameEvent("Z", "Y"); // Unrelated.
        ShadowGoogleAuthUtil.insertRenameEvent("A", "B");
        ShadowGoogleAuthUtil.insertRenameEvent("Y", "X"); // Unrelated.
        ShadowGoogleAuthUtil.insertRenameEvent("B", "C");
        ShadowGoogleAuthUtil.insertRenameEvent("C", "D");

        Assert.assertEquals("D", getNewNameOfRenamedAccount("A", List.of("D")));
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

        Assert.assertEquals("D", getNewNameOfRenamedAccount("A", List.of("D", "X")));
    }

    private @Nullable String getNewNameOfRenamedAccount(
            String oldAccountEmail, List<String> accountEmails) {
        final List<CoreAccountInfo> coreAccountInfos = new ArrayList<>();
        for (String email : accountEmails) {
            coreAccountInfos.add(CoreAccountInfo.createFromEmailAndGaiaId(email, "notUsedGaiaId"));
        }
        final AtomicReference<String> newAccountName = new AtomicReference<>();
        mChecker.getNewEmailOfRenamedAccountAsync(oldAccountEmail, coreAccountInfos)
                .then(newAccountName::set);
        return newAccountName.get();
    }
}
