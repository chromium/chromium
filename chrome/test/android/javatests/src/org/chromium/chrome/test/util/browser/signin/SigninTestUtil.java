// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser.signin;

import androidx.annotation.Nullable;
import androidx.annotation.WorkerThread;

import org.junit.Assert;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.SyncFirstSetupCompleteSource;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * Utility class for test signin functionality.
 */
public final class SigninTestUtil {
    /**
     * @return The primary account of the requested {@link ConsentLevel}.
     */
    static CoreAccountInfo getPrimaryAccount(@ConsentLevel int consentLevel) {
        return TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            return IdentityServicesProvider.get()
                    .getIdentityManager(Profile.getLastUsedRegularProfile())
                    .getPrimaryAccountInfo(consentLevel);
        });
    }

    /**
     * Signs the user into the given account.
     */
    public static void signin(CoreAccountInfo coreAccountInfo) {
        CallbackHelper callbackHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(
                    Profile.getLastUsedRegularProfile());
            signinManager.signin(AccountUtils.createAccountFromName(coreAccountInfo.getEmail()),
                    SigninAccessPoint.UNKNOWN, new SigninManager.SignInCallback() {
                        @Override
                        public void onSignInComplete() {
                            callbackHelper.notifyCalled();
                        }

                        @Override
                        public void onSignInAborted() {
                            Assert.fail("Sign-in was aborted");
                        }
                    });
        });
        try {
            callbackHelper.waitForFirst();
        } catch (TimeoutException e) {
            throw new RuntimeException("Timed out waiting for callback", e);
        }
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(coreAccountInfo,
                    IdentityServicesProvider.get()
                            .getIdentityManager(Profile.getLastUsedRegularProfile())
                            .getPrimaryAccountInfo(ConsentLevel.SIGNIN));
        });
    }

    /**
     * Signs into an account and enables the sync if given a {@link SyncService} object.
     *
     * @param syncService Enable the sync with it if it is not null.
     */
    @WorkerThread
    public static void signinAndEnableSync(
            CoreAccountInfo coreAccountInfo, @Nullable SyncService syncService) {
        CallbackHelper callbackHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(
                    Profile.getLastUsedRegularProfile());
            signinManager.signinAndEnableSync(
                    AccountUtils.createAccountFromName(coreAccountInfo.getEmail()),
                    SigninAccessPoint.UNKNOWN, new SigninManager.SignInCallback() {
                        @Override
                        public void onSignInComplete() {
                            if (syncService != null) {
                                syncService.setFirstSetupComplete(
                                        SyncFirstSetupCompleteSource.BASIC_FLOW);
                            }
                            callbackHelper.notifyCalled();
                        }

                        @Override
                        public void onSignInAborted() {
                            Assert.fail("Sign-in was aborted");
                        }
                    });
        });
        try {
            callbackHelper.waitForFirst();
        } catch (TimeoutException e) {
            throw new RuntimeException("Timed out waiting for callback", e);
        }
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(coreAccountInfo,
                    IdentityServicesProvider.get()
                            .getIdentityManager(Profile.getLastUsedRegularProfile())
                            .getPrimaryAccountInfo(ConsentLevel.SYNC));
        });
    }

    /**
     * Waits for the AccountTrackerService to seed system accounts.
     */
    static void seedAccounts() {
        ThreadUtils.assertOnBackgroundThread();
        CallbackHelper ch = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            IdentityServicesProvider.get()
                    .getAccountTrackerService(Profile.getLastUsedRegularProfile())
                    .seedAccountsIfNeeded(ch::notifyCalled);
        });
        try {
            ch.waitForFirst(
                    "Timed out while waiting for system accounts to seed.", 20, TimeUnit.SECONDS);
        } catch (TimeoutException ex) {
            throw new RuntimeException("Timed out while waiting for system accounts to seed.");
        }
    }

    static void signOut() {
        signOut(SignoutReason.TEST);
    }

    static void forceSignOut() {
        signOut(SignoutReason.FORCE_SIGNOUT_ALWAYS_ALLOWED_FOR_TEST);
    }

    private static void signOut(@SignoutReason int signoutReason) {
        ThreadUtils.assertOnBackgroundThread();
        CallbackHelper callbackHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            final SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(
                    Profile.getLastUsedRegularProfile());
            signinManager.runAfterOperationInProgress(
                    ()
                            -> signinManager.signOut(
                                    signoutReason, callbackHelper::notifyCalled, false));
        });
        try {
            callbackHelper.waitForFirst();
        } catch (TimeoutException e) {
            throw new RuntimeException("Timed out waiting for callback", e);
        }
    }

    private SigninTestUtil() {}
}
