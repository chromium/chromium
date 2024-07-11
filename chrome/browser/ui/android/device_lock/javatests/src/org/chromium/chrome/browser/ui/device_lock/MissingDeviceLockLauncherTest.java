// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ui.device_lock;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.app.KeyguardManager;
import android.content.Context;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManagerFactory;
import org.chromium.chrome.browser.password_manager.PasswordStoreBridge;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninManager.DataWipeOption;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.test.util.BlankUiTestActivity;

import java.util.concurrent.atomic.AtomicBoolean;

/** Tests for the {@link MissingDeviceLockLauncher}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class MissingDeviceLockLauncherTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Mock private KeyguardManager mKeyguardManager;
    @Mock private Context mContext;
    @Mock private MissingDeviceLockCoordinator mMissingDeviceLockCoordinator;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private IdentityServicesProvider mIdentityServicesProvider;
    @Mock private SigninManager mSigninManager;
    @Mock private IdentityManager mIdentityManager;
    @Mock private PersonalDataManager mPersonalDataManager;
    @Mock private Profile mProfile;
    @Mock private CoreAccountInfo mCoreAccountInfo;
    @Mock private PasswordStoreBridge mPasswordStoreBridge;

    private MissingDeviceLockLauncher mMissingDeviceLockLauncher;
    private SharedPreferencesManager mSharedPreferencesManager;
    private AtomicBoolean mWipeDataCallbackCalled = new AtomicBoolean();

    @Before
    public void setUp() {
        mKeyguardManager = Mockito.mock(KeyguardManager.class);
        mModalDialogManager = Mockito.mock(ModalDialogManager.class);
        mMissingDeviceLockCoordinator = Mockito.mock(MissingDeviceLockCoordinator.class);
        mIdentityServicesProvider = Mockito.mock(IdentityServicesProvider.class);
        mSigninManager = Mockito.mock(SigninManager.class);
        mIdentityManager = Mockito.mock(IdentityManager.class);
        mPersonalDataManager = Mockito.mock(PersonalDataManager.class);
        mProfile = Mockito.mock(Profile.class);
        mCoreAccountInfo = Mockito.mock(CoreAccountInfo.class);

        mSharedPreferencesManager = ChromeSharedPreferences.getInstance();
        mSharedPreferencesManager.removeKey(ChromePreferenceKeys.DEVICE_LOCK_SHOW_ALERT_IF_REMOVED);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        PersonalDataManagerFactory.setInstanceForTesting(mPersonalDataManager);
        mWipeDataCallbackCalled.set(false);

        mMissingDeviceLockLauncher =
                new MissingDeviceLockLauncher(mContext, mProfile, mModalDialogManager);
        mMissingDeviceLockLauncher.setPasswordStoreBridgeForTesting(mPasswordStoreBridge);

        doReturn(mKeyguardManager).when(mContext).getSystemService(eq(Context.KEYGUARD_SERVICE));
        doReturn(mSigninManager).when(mIdentityServicesProvider).getSigninManager(any());
        doReturn(mIdentityManager).when(mIdentityServicesProvider).getIdentityManager(any());
        doAnswer(
                        (invocation) -> {
                            Runnable runnable = invocation.getArgument(0);
                            runnable.run();
                            return null;
                        })
                .when(mSigninManager)
                .runAfterOperationInProgress(any());
    }

    @Test
    @MediumTest
    public void testCheckPrivateDataIsProtectedByDeviceLock_deviceIsSecure_nullMissingDeviceLock() {
        doReturn(true).when(mKeyguardManager).isDeviceSecure();

        assertNull(
                "The missing device lock dialog should not be shown when the device is secure.",
                mMissingDeviceLockLauncher.checkPrivateDataIsProtectedByDeviceLock());
        assertTrue(
                "The preference should be set to show the alert if the device lock is later "
                        + "removed.",
                mSharedPreferencesManager.readBoolean(
                        ChromePreferenceKeys.DEVICE_LOCK_SHOW_ALERT_IF_REMOVED,
                        /* defaultValue= */ false));
    }

    @Test
    @MediumTest
    public void testCheckPrivateDataIsProtectedByDeviceLock_deviceIsSecure() {
        doReturn(true).when(mKeyguardManager).isDeviceSecure();
        HistogramWatcher deviceLockRestoredHistogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Android.Automotive.DeviceLockRemovalDialogEvent",
                                MissingDeviceLockCoordinator.MissingDeviceLockDialogEvent
                                        .DEVICE_LOCK_RESTORED)
                        .build();

        mMissingDeviceLockLauncher.setMissingDeviceLockCoordinatorForTesting(
                mMissingDeviceLockCoordinator);

        assertNull(mMissingDeviceLockLauncher.checkPrivateDataIsProtectedByDeviceLock());
        deviceLockRestoredHistogram.assertExpected();
        verify(mMissingDeviceLockCoordinator, times(1)).hideDialog(anyInt());
        assertTrue(
                "The preference should be set to show the alert if the device lock is later "
                        + "removed.",
                mSharedPreferencesManager.readBoolean(
                        ChromePreferenceKeys.DEVICE_LOCK_SHOW_ALERT_IF_REMOVED,
                        /* defaultValue= */ false));
    }

    @Test
    @MediumTest
    public void testCheckPrivateDataIsProtectedByDeviceLock_showMissingDeviceLockDialog() {
        mActivityTestRule.setFinishActivity(true);
        mActivityTestRule.launchActivity(null);
        Activity activity = Mockito.spy(mActivityTestRule.getActivity());

        doReturn(mKeyguardManager).when(activity).getSystemService(eq(Context.KEYGUARD_SERVICE));
        doReturn(false).when(mKeyguardManager).isDeviceSecure();
        mSharedPreferencesManager.writeBoolean(
                ChromePreferenceKeys.DEVICE_LOCK_SHOW_ALERT_IF_REMOVED, true);

        MissingDeviceLockLauncher missingDeviceLockLauncher =
                new MissingDeviceLockLauncher(activity, mProfile, mModalDialogManager);
        MissingDeviceLockCoordinator missingDeviceLockCoordinator =
                missingDeviceLockLauncher.checkPrivateDataIsProtectedByDeviceLock();
        missingDeviceLockCoordinator.hideDialog(DialogDismissalCause.POSITIVE_BUTTON_CLICKED);

        assertNotNull(
                "The missing device dialog should have been created.",
                missingDeviceLockCoordinator);
        verify(mModalDialogManager, times(1)).showDialog(any(), anyInt(), anyInt());
        verify(mModalDialogManager, times(1)).dismissDialog(any(), anyInt());
        assertTrue(
                "The preference should still be set to show the alert after the missing "
                        + "device lock dialog is shown.",
                mSharedPreferencesManager.readBoolean(
                        ChromePreferenceKeys.DEVICE_LOCK_SHOW_ALERT_IF_REMOVED,
                        /* defaultValue= */ false));
    }

    @Test
    @MediumTest
    public void testEnsureSignOutAndDeleteSensitiveData_signedIn_wipeAllData() {
        mSharedPreferencesManager.writeBoolean(
                ChromePreferenceKeys.DEVICE_LOCK_SHOW_ALERT_IF_REMOVED, true);

        doReturn(mCoreAccountInfo).when(mIdentityManager).getPrimaryAccountInfo(anyInt());
        doAnswer(
                        (invocation) -> {
                            SigninManager.SignOutCallback callback = invocation.getArgument(1);
                            callback.signOutComplete();
                            return null;
                        })
                .when(mSigninManager)
                .signOut(anyInt(), any(), anyBoolean());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mMissingDeviceLockLauncher.ensureSignOutAndDeleteSensitiveData(
                            () -> mWipeDataCallbackCalled.set(true), /* wipeAllData= */ true);
                });
        verify(mSigninManager, times(1)).runAfterOperationInProgress(any());
        verify(mSigninManager, times(1)).signOut(anyInt(), any(), eq(true));
        verify(mSigninManager, times(0))
                .wipeSyncUserData(any(), eq(DataWipeOption.WIPE_ALL_PROFILE_DATA));
        verify(mPasswordStoreBridge, never()).clearAllPasswords();
        verify(mPersonalDataManager, never()).deleteAllLocalCreditCards();
        assertTrue(
                "The wipe data callback should have been called.", mWipeDataCallbackCalled.get());
        assertFalse(
                "The preference should be set to not show the device lock dialog again.",
                mSharedPreferencesManager.readBoolean(
                        ChromePreferenceKeys.DEVICE_LOCK_SHOW_ALERT_IF_REMOVED, true));
    }

    @Test
    @MediumTest
    public void testEnsureSignOutAndDeleteSensitiveData_signedIn_onlyWipePasswordsAndCreditCards() {
        mSharedPreferencesManager.writeBoolean(
                ChromePreferenceKeys.DEVICE_LOCK_SHOW_ALERT_IF_REMOVED, true);

        doReturn(mCoreAccountInfo).when(mIdentityManager).getPrimaryAccountInfo(anyInt());
        doAnswer(
                        (invocation) -> {
                            SigninManager.SignOutCallback callback = invocation.getArgument(1);
                            callback.signOutComplete();
                            return null;
                        })
                .when(mSigninManager)
                .signOut(anyInt(), any(), anyBoolean());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mMissingDeviceLockLauncher.ensureSignOutAndDeleteSensitiveData(
                            () -> mWipeDataCallbackCalled.set(true), /* wipeAllData= */ false);
                });
        verify(mSigninManager, times(1)).runAfterOperationInProgress(any());
        verify(mSigninManager, times(1)).signOut(anyInt(), any(), eq(false));
        verify(mSigninManager, times(0))
                .wipeSyncUserData(any(), eq(DataWipeOption.WIPE_ALL_PROFILE_DATA));
        verify(mPasswordStoreBridge, times(1)).clearAllPasswords();
        verify(mPersonalDataManager, times(1)).deleteAllLocalCreditCards();
        assertTrue(
                "The wipe data callback should have been called.", mWipeDataCallbackCalled.get());
        assertFalse(
                "The preference should be set to not show the device lock dialog again.",
                mSharedPreferencesManager.readBoolean(
                        ChromePreferenceKeys.DEVICE_LOCK_SHOW_ALERT_IF_REMOVED, true));
    }

    @Test
    @MediumTest
    public void testEnsureSignOutAndDeleteSensitiveData_notSignedIn_wipeAllData() {
        mSharedPreferencesManager.writeBoolean(
                ChromePreferenceKeys.DEVICE_LOCK_SHOW_ALERT_IF_REMOVED, true);

        doReturn(null).when(mIdentityManager).getPrimaryAccountInfo(anyInt());
        doAnswer(
                        (invocation) -> {
                            Runnable callback = invocation.getArgument(0);
                            callback.run();
                            return null;
                        })
                .when(mSigninManager)
                .wipeSyncUserData(any(), anyInt());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mMissingDeviceLockLauncher.ensureSignOutAndDeleteSensitiveData(
                            () -> mWipeDataCallbackCalled.set(true), /* wipeAllData= */ true);
                });
        verify(mSigninManager, times(1)).runAfterOperationInProgress(any());
        verify(mSigninManager, times(0)).signOut(anyInt(), any(), anyBoolean());
        verify(mSigninManager, times(1))
                .wipeSyncUserData(any(), eq(DataWipeOption.WIPE_ALL_PROFILE_DATA));
        verify(mPasswordStoreBridge, never()).clearAllPasswords();
        verify(mPersonalDataManager, never()).deleteAllLocalCreditCards();
        assertTrue(
                "The wipe data callback should have been called.", mWipeDataCallbackCalled.get());
        assertFalse(
                "The preference should be set to not show the device lock dialog again.",
                mSharedPreferencesManager.readBoolean(
                        ChromePreferenceKeys.DEVICE_LOCK_SHOW_ALERT_IF_REMOVED, true));
    }

    @Test
    @MediumTest
    public void
            testEnsureSignOutAndDeleteSensitiveData_notSignedIn_onlyWipePasswordsAndCreditCards() {
        mSharedPreferencesManager.writeBoolean(
                ChromePreferenceKeys.DEVICE_LOCK_SHOW_ALERT_IF_REMOVED, true);

        doReturn(null).when(mIdentityManager).getPrimaryAccountInfo(anyInt());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mMissingDeviceLockLauncher.ensureSignOutAndDeleteSensitiveData(
                            () -> mWipeDataCallbackCalled.set(true), /* wipeAllData= */ false);
                });
        verify(mSigninManager, times(1)).runAfterOperationInProgress(any());
        verify(mSigninManager, never()).signOut(anyInt(), any(), anyBoolean());
        verify(mSigninManager, never())
                .wipeSyncUserData(any(), eq(DataWipeOption.WIPE_ALL_PROFILE_DATA));
        verify(mPasswordStoreBridge, times(1)).clearAllPasswords();
        verify(mPersonalDataManager, times(1)).deleteAllLocalCreditCards();
        assertTrue(
                "The wipe data callback should have been called.", mWipeDataCallbackCalled.get());
        assertFalse(
                "The preference should be set to not show the device lock dialog again.",
                mSharedPreferencesManager.readBoolean(
                        ChromePreferenceKeys.DEVICE_LOCK_SHOW_ALERT_IF_REMOVED, true));
    }

    @Test
    @MediumTest
    public void testCheckPrivateDataIsProtectedByDeviceLock_noAction() {
        doReturn(true).when(mKeyguardManager).isDeviceSecure();
        mSharedPreferencesManager.writeBoolean(
                ChromePreferenceKeys.DEVICE_LOCK_SHOW_ALERT_IF_REMOVED, false);

        assertNull(
                "The Missing Device Lock dialog should not be created.",
                mMissingDeviceLockLauncher.checkPrivateDataIsProtectedByDeviceLock());
        verify(mModalDialogManager, never()).showDialog(any(), anyInt(), anyInt());
    }
}
