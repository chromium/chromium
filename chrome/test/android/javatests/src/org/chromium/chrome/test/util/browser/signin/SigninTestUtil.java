// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser.signin;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import androidx.annotation.Nullable;
import androidx.annotation.WorkerThread;

import org.junit.Assert;

import org.chromium.base.BuildInfo;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.signin.SigninFirstRunFragment;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.components.sync.SyncFirstSetupCompleteSource;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;

import java.util.concurrent.TimeoutException;

/** Utility class for test signin functionality. */
public final class SigninTestUtil {
    /**
     * @return The primary account of the requested {@link ConsentLevel}.
     */
    static CoreAccountInfo getPrimaryAccount(@ConsentLevel int consentLevel) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    return IdentityServicesProvider.get()
                            .getIdentityManager(ProfileManager.getLastUsedRegularProfile())
                            .getPrimaryAccountInfo(consentLevel);
                });
    }

    /** Signs the user into the given account. */
    public static void signin(CoreAccountInfo coreAccountInfo) {
        signin(coreAccountInfo, /* waitForPrefsCommit= */ false);
    }

    /** Signs the user into the given account and wait for the sign-in prefs to be committed */
    public static void signinAndWaitForPrefsCommit(CoreAccountInfo coreAccountInfo) {
        signin(coreAccountInfo, /* waitForPrefsCommit= */ true);
    }

    private static void signin(CoreAccountInfo coreAccountInfo, boolean waitForPrefsCommit) {
        CallbackHelper completionCallbackHelper = new CallbackHelper();
        CallbackHelper prefsCommitCallbackHelper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SigninManager signinManager =
                            IdentityServicesProvider.get()
                                    .getSigninManager(ProfileManager.getLastUsedRegularProfile());
                    signinManager.signin(
                            coreAccountInfo,
                            SigninAccessPoint.UNKNOWN,
                            new SigninManager.SignInCallback() {
                                @Override
                                public void onSignInComplete() {
                                    completionCallbackHelper.notifyCalled();
                                }

                                @Override
                                public void onPrefsCommitted() {
                                    prefsCommitCallbackHelper.notifyCalled();
                                }

                                @Override
                                public void onSignInAborted() {
                                    Assert.fail("Sign-in was aborted");
                                }
                            });
                });
        try {
            completionCallbackHelper.waitForOnly();
            if (waitForPrefsCommit) {
                prefsCommitCallbackHelper.waitForOnly();
            }
        } catch (TimeoutException e) {
            throw new RuntimeException("Timed out waiting for callback", e);
        }
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(coreAccountInfo, getPrimaryAccount(ConsentLevel.SIGNIN));
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
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SigninManager signinManager =
                            IdentityServicesProvider.get()
                                    .getSigninManager(ProfileManager.getLastUsedRegularProfile());
                    signinManager.signinAndEnableSync(
                            coreAccountInfo,
                            SigninAccessPoint.UNKNOWN,
                            new SigninManager.SignInCallback() {
                                @Override
                                public void onSignInComplete() {
                                    if (syncService != null) {
                                        syncService.setInitialSyncFeatureSetupComplete(
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
            callbackHelper.waitForOnly();
        } catch (TimeoutException e) {
            throw new RuntimeException("Timed out waiting for callback", e);
        }
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(coreAccountInfo, getPrimaryAccount(ConsentLevel.SYNC));
                });
    }

    /**
     * Signs into an account and enables history sync given a {@link SyncService} object.
     *
     * @param syncService Enable history sync with it.
     */
    @WorkerThread
    public static void signinAndEnableHistorySync(CoreAccountInfo coreAccountInfo) {
        CallbackHelper callbackHelper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SigninManager signinManager =
                            IdentityServicesProvider.get()
                                    .getSigninManager(ProfileManager.getLastUsedRegularProfile());
                    signinManager.signin(
                            coreAccountInfo,
                            SigninAccessPoint.UNKNOWN,
                            new SigninManager.SignInCallback() {
                                @Override
                                public void onSignInComplete() {
                                    SyncService syncService =
                                            SyncTestUtil.getSyncServiceForLastUsedProfile();
                                    syncService.setSelectedType(
                                            UserSelectableType.HISTORY, /* isTypeOn= */ true);
                                    syncService.setSelectedType(
                                            UserSelectableType.TABS, /* isTypeOn= */ true);
                                    callbackHelper.notifyCalled();
                                }

                                @Override
                                public void onSignInAborted() {
                                    Assert.fail("Sign-in was aborted");
                                }
                            });
                });
        try {
            callbackHelper.waitForOnly();
        } catch (TimeoutException e) {
            throw new RuntimeException("Timed out waiting for callback", e);
        }
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(coreAccountInfo, getPrimaryAccount(ConsentLevel.SIGNIN));
                });
        SyncTestUtil.waitForHistorySyncEnabled();
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
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    final SigninManager signinManager =
                            IdentityServicesProvider.get()
                                    .getSigninManager(ProfileManager.getLastUsedRegularProfile());
                    signinManager.runAfterOperationInProgress(
                            () ->
                                    signinManager.signOut(
                                            signoutReason, callbackHelper::notifyCalled, false));
                });
        try {
            callbackHelper.waitForOnly();
        } catch (TimeoutException e) {
            throw new RuntimeException("Timed out waiting for callback", e);
        }
    }

    /**
     * Simulates completing the device lock challenge for SigninFirstRunFragment.
     *
     * @param fragment The fragment under test.
     */
    public static void completeAutoDeviceLockIfNeeded(SigninFirstRunFragment fragment) {
        if (!ThreadUtils.runOnUiThreadBlocking(() -> BuildInfo.getInstance().isAutomotive)) {
            return;
        }

        onView(withId(R.id.device_lock_view)).check(matches(isDisplayed()));

        ThreadUtils.runOnUiThreadBlocking(() -> fragment.onDeviceLockReady());
    }

    private SigninTestUtil() {}
}
