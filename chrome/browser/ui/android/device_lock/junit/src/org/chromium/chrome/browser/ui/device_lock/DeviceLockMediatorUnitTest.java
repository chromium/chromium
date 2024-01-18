// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.device_lock;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ui.device_lock.DeviceLockProperties.DEVICE_SUPPORTS_PIN_CREATION_INTENT;
import static org.chromium.chrome.browser.ui.device_lock.DeviceLockProperties.ON_CREATE_DEVICE_LOCK_CLICKED;
import static org.chromium.chrome.browser.ui.device_lock.DeviceLockProperties.ON_DISMISS_CLICKED;
import static org.chromium.chrome.browser.ui.device_lock.DeviceLockProperties.ON_GO_TO_OS_SETTINGS_CLICKED;
import static org.chromium.chrome.browser.ui.device_lock.DeviceLockProperties.ON_USER_UNDERSTANDS_CLICKED;
import static org.chromium.chrome.browser.ui.device_lock.DeviceLockProperties.PREEXISTING_DEVICE_LOCK;
import static org.chromium.chrome.browser.ui.device_lock.DeviceLockProperties.UI_ENABLED;
import static org.chromium.components.browser_ui.device_lock.DeviceLockBridge.DEVICE_LOCK_PAGE_HAS_BEEN_PASSED;

import android.accounts.Account;
import android.app.Activity;
import android.app.KeyguardManager;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.ActivityInfo;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.stubbing.Answer;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.FeatureList;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.device_reauth.ReauthenticatorBridge;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.components.browser_ui.device_lock.DeviceLockDialogMetrics;
import org.chromium.components.signin.AccountReauthenticationUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for the {@link DeviceLockMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Features.EnableFeatures({ChromeFeatureList.ACCOUNT_REAUTHENTICATION_RECENT_TIME_WINDOW})
public class DeviceLockMediatorUnitTest {
    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    @Mock public Activity mActivity;
    @Mock public Account mAccount;
    @Mock private DeviceLockCoordinator.Delegate mDelegate;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private ReauthenticatorBridge mDeviceLockAuthenticatorBridge;
    @Mock private AccountReauthenticationUtils mAccountReauthenticationUtils;
    @Mock private KeyguardManager mKeyguardManager;
    @Mock private PackageManager mPackageManager;
    @Mock private View mView;

    private final Answer<Object> mSuccessfulDeviceLockCreation =
            (invocation) -> {
                WindowAndroid.IntentCallback callback = invocation.getArgument(1);
                doReturn(true).when(mKeyguardManager).isDeviceSecure();
                callback.onIntentCompleted(Activity.RESULT_OK, new Intent());
                return null;
            };

    private final Answer<Object> mFailedDeviceLockCreation =
            (invocation) -> {
                WindowAndroid.IntentCallback callback = invocation.getArgument(1);
                doReturn(false).when(mKeyguardManager).isDeviceSecure();
                callback.onIntentCompleted(Activity.RESULT_CANCELED, new Intent());
                return null;
            };

    private final Answer<Object> mSuccessfulDeviceLockChallenge =
            (invocation) -> {
                Callback<Boolean> callback = invocation.getArgument(0);
                callback.onResult(true);
                return null;
            };

    private final Answer<Object> mFailedDeviceLockChallenge =
            (invocation) -> {
                Callback<Boolean> callback = invocation.getArgument(0);
                callback.onResult(false);
                return null;
            };

    private final Answer<Object> mSuccessfulAccountReauthenticationChallenge =
            (invocation) -> {
                @AccountReauthenticationUtils.ConfirmationResult
                Callback<Integer> callback = invocation.getArgument(3);
                callback.onResult(AccountReauthenticationUtils.ConfirmationResult.SUCCESS);
                return null;
            };

    private final Answer<Object> mRejectedAccountReauthenticationChallenge =
            (invocation) -> {
                @AccountReauthenticationUtils.ConfirmationResult
                Callback<Integer> callback = invocation.getArgument(3);
                callback.onResult(AccountReauthenticationUtils.ConfirmationResult.REJECTED);
                return null;
            };

    private final Answer<Object> mErrorAccountReauthenticationChallenge =
            (invocation) -> {
                @AccountReauthenticationUtils.ConfirmationResult
                Callback<Integer> callback = invocation.getArgument(3);
                callback.onResult(AccountReauthenticationUtils.ConfirmationResult.ERROR);
                return null;
            };

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        when(mDelegate.getSource()).thenReturn(DeviceLockActivityLauncher.Source.AUTOFILL);
        doReturn(mKeyguardManager).when(mActivity).getSystemService(eq(Context.KEYGUARD_SERVICE));
        doReturn(mPackageManager).when(mActivity).getPackageManager();

        FeatureList.TestValues testValues = new FeatureList.TestValues();
        testValues.addFieldTrialParamOverride(
                ChromeFeatureList.ACCOUNT_REAUTHENTICATION_RECENT_TIME_WINDOW,
                DeviceLockMediator.ACCOUNT_REAUTHENTICATION_RECENT_TIME_WINDOW_PARAM,
                "10");
        FeatureList.setTestValues(testValues);
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        prefs.edit().remove(DEVICE_LOCK_PAGE_HAS_BEEN_PASSED).apply();
    }

    @Test
    public void testDeviceLockMediator_deviceSecure_preExistingDeviceLockIsTrue() {
        doReturn(true).when(mKeyguardManager).isDeviceSecure();

        DeviceLockMediator deviceLockMediator =
                new DeviceLockMediator(
                        mDelegate,
                        null,
                        mDeviceLockAuthenticatorBridge,
                        mAccountReauthenticationUtils,
                        mActivity,
                        mAccount);

        assertTrue(
                "PropertyModel PREEXISTING_DEVICE_LOCK should be True",
                deviceLockMediator.getModel().get(PREEXISTING_DEVICE_LOCK));
    }

    @Test
    public void testDeviceLockMediator_deviceNotSecure_preExistingDeviceLockIsFalse() {
        doReturn(false).when(mKeyguardManager).isDeviceSecure();

        DeviceLockMediator deviceLockMediator =
                new DeviceLockMediator(
                        mDelegate,
                        null,
                        mDeviceLockAuthenticatorBridge,
                        mAccountReauthenticationUtils,
                        mActivity,
                        mAccount);

        assertFalse(
                "PropertyModel PREEXISTING_DEVICE_LOCK should be True",
                deviceLockMediator.getModel().get(PREEXISTING_DEVICE_LOCK));
    }

    @Test
    public void
            testDeviceLockMediator_deviceLockCreationIntentSupported_deviceSupportsPINIntentIsTrue() {
        ResolveInfo resolveInfo = new ResolveInfo();
        resolveInfo.isDefault = true;

        ApplicationInfo applicationInfo = new ApplicationInfo();
        applicationInfo.packageName = "example.package";
        resolveInfo.activityInfo = new ActivityInfo();
        resolveInfo.activityInfo.applicationInfo = applicationInfo;
        resolveInfo.activityInfo.name = "ExamplePackage";

        doReturn(resolveInfo).when(mPackageManager).resolveActivity(any(), anyInt());
        DeviceLockMediator deviceLockMediator =
                new DeviceLockMediator(
                        mDelegate,
                        null,
                        mDeviceLockAuthenticatorBridge,
                        mAccountReauthenticationUtils,
                        mActivity,
                        mAccount);

        assertTrue(
                "PropertyModel DEVICE_SUPPORTS_PIN_CREATION_INTENT should be True",
                deviceLockMediator.getModel().get(DEVICE_SUPPORTS_PIN_CREATION_INTENT));
    }

    @Test
    public void
            testDeviceLockMediator_deviceLockCreationIntentNotSupported_deviceSupportsPINIntentIsFalse() {
        DeviceLockMediator deviceLockMediator =
                new DeviceLockMediator(
                        mDelegate,
                        null,
                        mDeviceLockAuthenticatorBridge,
                        mAccountReauthenticationUtils,
                        mActivity,
                        mAccount);

        assertFalse(
                "PropertyModel DEVICE_SUPPORTS_PIN_CREATION_INTENT should be False",
                deviceLockMediator.getModel().get(DEVICE_SUPPORTS_PIN_CREATION_INTENT));
    }

    @Test
    public void
            testCreateDeviceLockOnClick_deviceLockCreatedSuccessfully_callsDelegateOnDeviceLockReady() {
        testOnClick(
                mAccount,
                mDeviceLockAuthenticatorBridge,
                ON_CREATE_DEVICE_LOCK_CLICKED,
                mSuccessfulDeviceLockCreation,
                /* deviceLockChallengeResult= */ null,
                mSuccessfulAccountReauthenticationChallenge,
                /* deviceLockCreationCalls= */ 1,
                /* deviceLockChallengesTriggered= */ 0,
                /* accountReauthenticationsTriggered= */ 1,
                /* onDeviceLockReadyCalls= */ 1,
                /* onDeviceLockRefusedCalls= */ 0);
    }

    @Test
    public void testCreateDeviceLockOnClick_nullAccount_noReauthenticationTriggered() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                DeviceLockDialogMetrics.DEVICE_LOCK_DIALOG_ACTION_HISTOGRAM_PREFIX
                                        + mDelegate.getSource(),
                                DeviceLockDialogMetrics.DeviceLockDialogAction
                                        .CREATE_DEVICE_LOCK_CLICKED)
                        .build();

        testOnClick(
                null,
                mDeviceLockAuthenticatorBridge,
                ON_CREATE_DEVICE_LOCK_CLICKED,
                mSuccessfulDeviceLockCreation,
                /* deviceLockChallengeResult= */ null,
                mSuccessfulAccountReauthenticationChallenge,
                /* deviceLockCreationCalls= */ 1,
                /* deviceLockChallengesTriggered= */ 0,
                /* accountReauthenticationsTriggered= */ 0,
                /* onDeviceLockReadyCalls= */ 1,
                /* onDeviceLockRefusedCalls= */ 0);

        histogramWatcher.assertExpected();
    }

    @Test
    public void
            testCreateDeviceLockOnClick_previouslySetDeviceLock_callsDelegateOnDeviceLockReady() {
        doReturn(true).when(mKeyguardManager).isDeviceSecure();
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                DeviceLockDialogMetrics.DEVICE_LOCK_DIALOG_ACTION_HISTOGRAM_PREFIX
                                        + mDelegate.getSource(),
                                DeviceLockDialogMetrics.DeviceLockDialogAction
                                        .CREATE_DEVICE_LOCK_CLICKED)
                        .build();

        testOnClick(
                mAccount,
                mDeviceLockAuthenticatorBridge,
                ON_CREATE_DEVICE_LOCK_CLICKED,
                /* deviceLockCreationResult= */ null,
                /* deviceLockChallengeResult= */ null,
                mSuccessfulAccountReauthenticationChallenge,
                /* deviceLockCreationCalls= */ 0,
                /* deviceLockChallengesTriggered= */ 0,
                /* accountReauthenticationsTriggered= */ 1,
                /* onDeviceLockReadyCalls= */ 1,
                /* onDeviceLockRefusedCalls= */ 0);

        histogramWatcher.assertExpected();
    }

    @Test
    public void testCreateDeviceLockOnClick_noDeviceLockCreated_noDelegateCalls() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                DeviceLockDialogMetrics.DEVICE_LOCK_DIALOG_ACTION_HISTOGRAM_PREFIX
                                        + mDelegate.getSource(),
                                DeviceLockDialogMetrics.DeviceLockDialogAction
                                        .CREATE_DEVICE_LOCK_CLICKED)
                        .build();

        testOnClick(
                mAccount,
                mDeviceLockAuthenticatorBridge,
                ON_CREATE_DEVICE_LOCK_CLICKED,
                mFailedDeviceLockCreation,
                /* deviceLockChallengeResult= */ null,
                /* accountReauthenticationResult= */ null,
                /* deviceLockCreationCalls= */ 1,
                /* deviceLockChallengesTriggered= */ 0,
                /* accountReauthenticationsTriggered= */ 0,
                /* onDeviceLockReadyCalls= */ 0,
                /* onDeviceLockRefusedCalls= */ 0);

        histogramWatcher.assertExpected();
    }

    @Test
    public void testCreateDeviceLockOnClick_rejectedAccountReauthentication_noDelegateCalls() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                DeviceLockDialogMetrics.DEVICE_LOCK_DIALOG_ACTION_HISTOGRAM_PREFIX
                                        + mDelegate.getSource(),
                                DeviceLockDialogMetrics.DeviceLockDialogAction
                                        .CREATE_DEVICE_LOCK_CLICKED)
                        .build();

        testOnClick(
                mAccount,
                mDeviceLockAuthenticatorBridge,
                ON_CREATE_DEVICE_LOCK_CLICKED,
                mSuccessfulDeviceLockCreation,
                /* deviceLockChallengeResult= */ null,
                mRejectedAccountReauthenticationChallenge,
                /* deviceLockCreationCalls= */ 1,
                /* deviceLockChallengesTriggered= */ 0,
                /* accountReauthenticationsTriggered= */ 1,
                /* onDeviceLockReadyCalls= */ 0,
                /* onDeviceLockRefusedCalls= */ 0);

        histogramWatcher.assertExpected();
    }

    @Test
    public void
            testGoToOSSettingsOnClick_deviceLockCreatedSuccessfully_callsDelegateOnDeviceLockReady() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                DeviceLockDialogMetrics.DEVICE_LOCK_DIALOG_ACTION_HISTOGRAM_PREFIX
                                        + mDelegate.getSource(),
                                DeviceLockDialogMetrics.DeviceLockDialogAction
                                        .GO_TO_OS_SETTINGS_CLICKED)
                        .build();

        testOnClick(
                mAccount,
                mDeviceLockAuthenticatorBridge,
                ON_GO_TO_OS_SETTINGS_CLICKED,
                mSuccessfulDeviceLockCreation,
                /* deviceLockChallengeResult= */ null,
                mSuccessfulAccountReauthenticationChallenge,
                /* deviceLockCreationCalls= */ 1,
                /* deviceLockChallengesTriggered= */ 0,
                /* accountReauthenticationsTriggered= */ 1,
                /* onDeviceLockReadyCalls= */ 1,
                /* onDeviceLockRefusedCalls= */ 0);

        histogramWatcher.assertExpected();
    }

    @Test
    public void testGoToOSSettingsOnClick_previouslySetDeviceLock_callsDelegateOnDeviceLockReady() {
        doReturn(true).when(mKeyguardManager).isDeviceSecure();
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                DeviceLockDialogMetrics.DEVICE_LOCK_DIALOG_ACTION_HISTOGRAM_PREFIX
                                        + mDelegate.getSource(),
                                DeviceLockDialogMetrics.DeviceLockDialogAction
                                        .GO_TO_OS_SETTINGS_CLICKED)
                        .build();

        testOnClick(
                mAccount,
                mDeviceLockAuthenticatorBridge,
                ON_GO_TO_OS_SETTINGS_CLICKED,
                /* deviceLockCreationResult= */ null,
                /* deviceLockChallengeResult= */ null,
                mSuccessfulAccountReauthenticationChallenge,
                /* deviceLockCreationCalls= */ 0,
                /* deviceLockChallengesTriggered= */ 0,
                /* accountReauthenticationsTriggered= */ 1,
                /* onDeviceLockReadyCalls= */ 1,
                /* onDeviceLockRefusedCalls= */ 0);

        histogramWatcher.assertExpected();
    }

    @Test
    public void testGoToOSSettingsOnClick_noDeviceLockCreated_noDelegateCalls() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                DeviceLockDialogMetrics.DEVICE_LOCK_DIALOG_ACTION_HISTOGRAM_PREFIX
                                        + mDelegate.getSource(),
                                DeviceLockDialogMetrics.DeviceLockDialogAction
                                        .GO_TO_OS_SETTINGS_CLICKED)
                        .build();

        testOnClick(
                mAccount,
                mDeviceLockAuthenticatorBridge,
                ON_GO_TO_OS_SETTINGS_CLICKED,
                mFailedDeviceLockCreation,
                /* deviceLockChallengeResult= */ null,
                /* accountReauthenticationResult= */ null,
                /* deviceLockCreationCalls= */ 1,
                /* deviceLockChallengesTriggered= */ 0,
                /* accountReauthenticationsTriggered= */ 0,
                /* onDeviceLockReadyCalls= */ 0,
                /* onDeviceLockRefusedCalls= */ 0);

        histogramWatcher.assertExpected();
    }

    @Test
    public void testGoToOSSettingsOnClick_rejectedAccountReauthentication_noDelegateCalls() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                DeviceLockDialogMetrics.DEVICE_LOCK_DIALOG_ACTION_HISTOGRAM_PREFIX
                                        + mDelegate.getSource(),
                                DeviceLockDialogMetrics.DeviceLockDialogAction
                                        .GO_TO_OS_SETTINGS_CLICKED)
                        .build();

        testOnClick(
                mAccount,
                mDeviceLockAuthenticatorBridge,
                ON_GO_TO_OS_SETTINGS_CLICKED,
                mSuccessfulDeviceLockCreation,
                /* deviceLockChallengeResult= */ null,
                mRejectedAccountReauthenticationChallenge,
                /* deviceLockCreationCalls= */ 1,
                /* deviceLockChallengesTriggered= */ 0,
                /* accountReauthenticationsTriggered= */ 1,
                /* onDeviceLockReadyCalls= */ 0,
                /* onDeviceLockRefusedCalls= */ 0);

        histogramWatcher.assertExpected();
    }

    @Test
    public void testUserUnderstandsOnClick_successfulChallenges_callsDelegateOnDeviceLockReady() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                DeviceLockDialogMetrics.DEVICE_LOCK_DIALOG_ACTION_HISTOGRAM_PREFIX
                                        + mDelegate.getSource(),
                                DeviceLockDialogMetrics.DeviceLockDialogAction
                                        .USER_UNDERSTANDS_CLICKED)
                        .build();

        testOnClick(
                mAccount,
                mDeviceLockAuthenticatorBridge,
                ON_USER_UNDERSTANDS_CLICKED,
                /* deviceLockCreationResult= */ null,
                mSuccessfulDeviceLockChallenge,
                mSuccessfulAccountReauthenticationChallenge,
                /* deviceLockCreationCalls= */ 0,
                /* deviceLockChallengesTriggered= */ 1,
                /* accountReauthenticationsTriggered= */ 1,
                /* onDeviceLockReadyCalls= */ 1,
                /* onDeviceLockRefusedCalls= */ 0);

        histogramWatcher.assertExpected();
    }

    @Test
    public void
            testUserUnderstandsOnClick_nullReauthenticationBridge_noReauthenticationChallenge() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                DeviceLockDialogMetrics.DEVICE_LOCK_DIALOG_ACTION_HISTOGRAM_PREFIX
                                        + mDelegate.getSource(),
                                DeviceLockDialogMetrics.DeviceLockDialogAction
                                        .USER_UNDERSTANDS_CLICKED)
                        .build();

        testOnClick(
                mAccount,
                /* deviceLockAuthenticatorBridge= */ null,
                ON_USER_UNDERSTANDS_CLICKED,
                /* deviceLockCreationResult= */ null,
                /* deviceLockChallengeResult= */ null,
                mSuccessfulAccountReauthenticationChallenge,
                /* deviceLockCreationCalls= */ 0,
                /* deviceLockChallengesTriggered= */ 0,
                /* accountReauthenticationsTriggered= */ 1,
                /* onDeviceLockReadyCalls= */ 1,
                /* onDeviceLockRefusedCalls= */ 0);

        histogramWatcher.assertExpected();
    }

    @Test
    public void testUserUnderstandsOnClick_failedDeviceLockChallenge_noDelegateCalls() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                DeviceLockDialogMetrics.DEVICE_LOCK_DIALOG_ACTION_HISTOGRAM_PREFIX
                                        + mDelegate.getSource(),
                                DeviceLockDialogMetrics.DeviceLockDialogAction
                                        .USER_UNDERSTANDS_CLICKED)
                        .build();

        testOnClick(
                mAccount,
                mDeviceLockAuthenticatorBridge,
                ON_USER_UNDERSTANDS_CLICKED,
                /* deviceLockCreationResult= */ null,
                mFailedDeviceLockChallenge,
                /* accountReauthenticationResult= */ null,
                /* deviceLockCreationCalls= */ 0,
                /* deviceLockChallengesTriggered= */ 1,
                /* accountReauthenticationsTriggered= */ 0,
                /* onDeviceLockReadyCalls= */ 0,
                /* onDeviceLockRefusedCalls= */ 0);

        histogramWatcher.assertExpected();
    }

    @Test
    public void testUserUnderstandsOnClick_rejectedAccountReauthentication_noDelegateCalls() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                DeviceLockDialogMetrics.DEVICE_LOCK_DIALOG_ACTION_HISTOGRAM_PREFIX
                                        + mDelegate.getSource(),
                                DeviceLockDialogMetrics.DeviceLockDialogAction
                                        .USER_UNDERSTANDS_CLICKED)
                        .build();

        testOnClick(
                mAccount,
                mDeviceLockAuthenticatorBridge,
                ON_USER_UNDERSTANDS_CLICKED,
                /* deviceLockCreationResult= */ null,
                mSuccessfulDeviceLockChallenge,
                mRejectedAccountReauthenticationChallenge,
                /* deviceLockCreationCalls= */ 0,
                /* deviceLockChallengesTriggered= */ 1,
                /* accountReauthenticationsTriggered= */ 1,
                /* onDeviceLockReadyCalls= */ 0,
                /* onDeviceLockRefusedCalls= */ 0);

        histogramWatcher.assertExpected();
    }

    @Test
    public void testUserUnderstandsOnClick_errorAccountReauthentication_noDelegateCalls() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                DeviceLockDialogMetrics.DEVICE_LOCK_DIALOG_ACTION_HISTOGRAM_PREFIX
                                        + mDelegate.getSource(),
                                DeviceLockDialogMetrics.DeviceLockDialogAction
                                        .USER_UNDERSTANDS_CLICKED)
                        .build();

        testOnClick(
                mAccount,
                mDeviceLockAuthenticatorBridge,
                ON_USER_UNDERSTANDS_CLICKED,
                /* deviceLockCreationResult= */ null,
                mSuccessfulDeviceLockChallenge,
                mErrorAccountReauthenticationChallenge,
                /* deviceLockCreationCalls= */ 0,
                /* deviceLockChallengesTriggered= */ 1,
                /* accountReauthenticationsTriggered= */ 1,
                /* onDeviceLockReadyCalls= */ 0,
                /* onDeviceLockRefusedCalls= */ 0);

        histogramWatcher.assertExpected();
    }

    @Test
    public void testDeviceLockMediator_dismissOnClick_callsDelegateOnDeviceLockRefused() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                DeviceLockDialogMetrics.DEVICE_LOCK_DIALOG_ACTION_HISTOGRAM_PREFIX
                                        + mDelegate.getSource(),
                                DeviceLockDialogMetrics.DeviceLockDialogAction.DISMISS_CLICKED)
                        .build();

        testOnClick(
                mAccount,
                mDeviceLockAuthenticatorBridge,
                ON_DISMISS_CLICKED,
                /* deviceLockCreationResult= */ null,
                /* deviceLockChallengeResult= */ null,
                /* accountReauthenticationResult= */ null,
                /* deviceLockCreationCalls= */ 0,
                /* deviceLockChallengesTriggered= */ 0,
                /* accountReauthenticationsTriggered= */ 0,
                /* onDeviceLockReadyCalls= */ 0,
                /* onDeviceLockRefusedCalls= */ 1);

        histogramWatcher.assertExpected();
    }

    private void testOnClick(
            Account account,
            ReauthenticatorBridge deviceLockAuthenticatorBridge,
            PropertyModel.ReadableObjectPropertyKey<View.OnClickListener> onClick,
            Answer<Object> deviceLockCreationResult,
            Answer<Object> deviceLockChallengeResult,
            Answer<Object> accountReauthenticationResult,
            int deviceLockCreationCalls,
            int deviceLockChallengesTriggered,
            int accountReauthenticationsTriggered,
            int onDeviceLockReadyCalls,
            int onDeviceLockRefusedCalls) {
        if (deviceLockCreationResult != null) {
            doAnswer(deviceLockCreationResult)
                    .when(mWindowAndroid)
                    .showIntent(
                            any(Intent.class), any(WindowAndroid.IntentCallback.class), isNull());
        }
        if (deviceLockChallengeResult != null) {
            doAnswer(deviceLockChallengeResult)
                    .when(deviceLockAuthenticatorBridge)
                    .reauthenticate(any());
        }
        if (accountReauthenticationResult != null) {
            doAnswer(accountReauthenticationResult)
                    .when(mAccountReauthenticationUtils)
                    .confirmCredentialsOrRecentAuthentication(
                            any(), any(), any(), any(), anyLong());
        }

        DeviceLockMediator deviceLockMediator =
                new DeviceLockMediator(
                        mDelegate,
                        mWindowAndroid,
                        deviceLockAuthenticatorBridge,
                        mAccountReauthenticationUtils,
                        mActivity,
                        account);
        deviceLockMediator.getModel().get(onClick).onClick(mView);

        verify(mWindowAndroid, times(deviceLockCreationCalls))
                .showIntent(any(Intent.class), any(WindowAndroid.IntentCallback.class), any());
        verify(mDeviceLockAuthenticatorBridge, times(deviceLockChallengesTriggered))
                .reauthenticate(any());
        verify(mAccountReauthenticationUtils, times(accountReauthenticationsTriggered))
                .confirmCredentialsOrRecentAuthentication(any(), any(), any(), any(), anyLong());
        verify(mDelegate, times(onDeviceLockReadyCalls)).onDeviceLockReady();
        verify(mDelegate, times(onDeviceLockRefusedCalls)).onDeviceLockRefused();

        if (onDeviceLockReadyCalls > 0) {
            assertTrue(
                    "Chrome should have recorded as passing the device lock page if the "
                            + "device lock is ready.",
                    ContextUtils.getAppSharedPreferences()
                            .getBoolean(DEVICE_LOCK_PAGE_HAS_BEEN_PASSED, false));
        } else {
            assertFalse(
                    "Chrome should not have recorded as passing the device lock page if the "
                            + "device lock is not ready.",
                    ContextUtils.getAppSharedPreferences()
                            .getBoolean(DEVICE_LOCK_PAGE_HAS_BEEN_PASSED, false));
        }
        // The UI should only be disabled if the user chose to continue and passed all the
        // necessary steps - the UI should be enabled if the user chooses to dismiss or fails the
        // challenges.
        if (onClick.equals(ON_DISMISS_CLICKED) || (onDeviceLockReadyCalls < 1)) {
            assertTrue(
                    "The UI should still be in an enabled state.",
                    deviceLockMediator.getModel().get(UI_ENABLED));
        } else {
            assertFalse(
                    "The UI should have been set to a disabled state.",
                    deviceLockMediator.getModel().get(UI_ENABLED));
        }
    }
}
