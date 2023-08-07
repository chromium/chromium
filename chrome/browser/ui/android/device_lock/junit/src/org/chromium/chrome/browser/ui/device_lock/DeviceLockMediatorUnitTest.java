// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.device_lock;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.ui.device_lock.DeviceLockProperties.DEVICE_SUPPORTS_PIN_CREATION_INTENT;
import static org.chromium.chrome.browser.ui.device_lock.DeviceLockProperties.IN_SIGN_IN_FLOW;
import static org.chromium.chrome.browser.ui.device_lock.DeviceLockProperties.ON_CREATE_DEVICE_LOCK_CLICKED;
import static org.chromium.chrome.browser.ui.device_lock.DeviceLockProperties.ON_DISMISS_CLICKED;
import static org.chromium.chrome.browser.ui.device_lock.DeviceLockProperties.ON_GO_TO_OS_SETTINGS_CLICKED;
import static org.chromium.chrome.browser.ui.device_lock.DeviceLockProperties.ON_USER_UNDERSTANDS_CLICKED;
import static org.chromium.chrome.browser.ui.device_lock.DeviceLockProperties.PREEXISTING_DEVICE_LOCK;

import android.accounts.Account;
import android.app.Activity;
import android.app.KeyguardManager;
import android.app.admin.DevicePolicyManager;
import android.content.Context;
import android.content.Intent;
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
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.mockito.stubbing.Answer;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.device_reauth.ReauthenticatorBridge;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.signin.AccountReauthenticationUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.BlankUiTestActivity;

/** Unit tests for the {@link DeviceLockMediator}.*/
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DeviceLockMediatorUnitTest {
    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();
    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Mock
    public Activity mActivity;
    @Mock
    public Account mAccount;
    @Mock
    private MockDelegate mDelegate;
    @Mock
    private WindowAndroid mWindowAndroid;
    @Mock
    private ReauthenticatorBridge mDeviceLockAuthenticatorBridge;
    @Mock
    private AccountReauthenticationUtils mAccountReauthenticationUtils;
    @Mock
    private KeyguardManager mKeyguardManager;
    @Mock
    private PackageManager mPackageManager;
    @Mock
    private View mMockView;

    private final Answer<Object> mSuccessfulDeviceLockCreation = (invocation) -> {
        WindowAndroid.IntentCallback callback = invocation.getArgument(1);
        doReturn(true).when(mKeyguardManager).isDeviceSecure();
        callback.onIntentCompleted(Activity.RESULT_OK, new Intent());
        return null;
    };

    private final Answer<Object> mFailedDeviceLockCreation = (invocation) -> {
        WindowAndroid.IntentCallback callback = invocation.getArgument(1);
        doReturn(false).when(mKeyguardManager).isDeviceSecure();
        callback.onIntentCompleted(Activity.RESULT_CANCELED, new Intent());
        return null;
    };

    private final Answer<Object> mSuccessfulDeviceLockChallenge = (invocation) -> {
        Callback<Boolean> callback = invocation.getArgument(0);
        callback.onResult(true);
        return null;
    };

    private final Answer<Object> mFailedDeviceLockChallenge = (invocation) -> {
        Callback<Boolean> callback = invocation.getArgument(0);
        callback.onResult(false);
        return null;
    };

    private final Answer<Object> mSuccessfulAccountReauthenticationChallenge = (invocation) -> {
        @AccountReauthenticationUtils.ConfirmationResult
        Callback<Integer> callback = invocation.getArgument(3);
        callback.onResult(AccountReauthenticationUtils.ConfirmationResult.SUCCESS);
        return null;
    };

    private final Answer<Object> mRejectedAccountReauthenticationChallenge = (invocation) -> {
        @AccountReauthenticationUtils.ConfirmationResult
        Callback<Integer> callback = invocation.getArgument(3);
        callback.onResult(AccountReauthenticationUtils.ConfirmationResult.REJECTED);
        return null;
    };

    private final Answer<Object> mErrorAccountReauthenticationChallenge = (invocation) -> {
        @AccountReauthenticationUtils.ConfirmationResult
        Callback<Integer> callback = invocation.getArgument(3);
        callback.onResult(AccountReauthenticationUtils.ConfirmationResult.ERROR);
        return null;
    };

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivity = Mockito.mock(Activity.class);
        mDelegate = Mockito.mock(MockDelegate.class);
        mMockView = Mockito.mock(View.class);

        mKeyguardManager = Mockito.mock(KeyguardManager.class);
        mPackageManager = Mockito.mock(PackageManager.class);
        doReturn(mKeyguardManager).when(mActivity).getSystemService(eq(Context.KEYGUARD_SERVICE));
        doReturn(mPackageManager).when(mActivity).getPackageManager();
    }

    @Test
    public void testDeviceLockMediator_deviceSecure_preExistingDeviceLockIsTrue() {
        doReturn(true).when(mKeyguardManager).isDeviceSecure();

        DeviceLockMediator deviceLockMediator = new DeviceLockMediator(mDelegate, null,
                mDeviceLockAuthenticatorBridge, mAccountReauthenticationUtils, mActivity, mAccount);

        assertTrue("PropertyModel PREEXISTING_DEVICE_LOCK should be True",
                deviceLockMediator.getModel().get(PREEXISTING_DEVICE_LOCK));
    }

    @Test
    public void testDeviceLockMediator_deviceNotSecure_preExistingDeviceLockIsFalse() {
        doReturn(false).when(mKeyguardManager).isDeviceSecure();

        DeviceLockMediator deviceLockMediator = new DeviceLockMediator(mDelegate, null,
                mDeviceLockAuthenticatorBridge, mAccountReauthenticationUtils, mActivity, mAccount);

        assertFalse("PropertyModel PREEXISTING_DEVICE_LOCK should be True",
                deviceLockMediator.getModel().get(PREEXISTING_DEVICE_LOCK));
    }

    @Test
    public void
    testDeviceLockMediator_deviceLockCreationIntentSupported_deviceSupportsPINIntentIsTrue() {
        Intent intent = new Intent(DevicePolicyManager.ACTION_SET_NEW_PASSWORD);
        ResolveInfo resolveInfo = new ResolveInfo();
        resolveInfo.isDefault = true;

        ApplicationInfo applicationInfo = new ApplicationInfo();
        applicationInfo.packageName = "example.package";
        resolveInfo.activityInfo = new ActivityInfo();
        resolveInfo.activityInfo.applicationInfo = applicationInfo;
        resolveInfo.activityInfo.name = "ExamplePackage";

        doReturn(resolveInfo).when(mPackageManager).resolveActivity(any(), anyInt());
        DeviceLockMediator deviceLockMediator = new DeviceLockMediator(mDelegate, null,
                mDeviceLockAuthenticatorBridge, mAccountReauthenticationUtils, mActivity, mAccount);

        assertTrue("PropertyModel DEVICE_SUPPORTS_PIN_CREATION_INTENT should be True",
                deviceLockMediator.getModel().get(DEVICE_SUPPORTS_PIN_CREATION_INTENT));
    }

    @Test
    public void
    testDeviceLockMediator_deviceLockCreationIntentNotSupported_deviceSupportsPINIntentIsFalse() {
        DeviceLockMediator deviceLockMediator = new DeviceLockMediator(mDelegate, null,
                mDeviceLockAuthenticatorBridge, mAccountReauthenticationUtils, mActivity, mAccount);

        assertFalse("PropertyModel DEVICE_SUPPORTS_PIN_CREATION_INTENT should be False",
                deviceLockMediator.getModel().get(DEVICE_SUPPORTS_PIN_CREATION_INTENT));
    }

    @Test
    public void testDeviceLockMediator_inSignInFlow_inSignInFlowIsTrue() {
        DeviceLockMediator deviceLockMediator = new DeviceLockMediator(mDelegate, null,
                mDeviceLockAuthenticatorBridge, mAccountReauthenticationUtils, mActivity, mAccount);

        assertTrue("PropertyModel IN_SIGN_IN_FLOW should be True",
                deviceLockMediator.getModel().get(IN_SIGN_IN_FLOW));
    }

    @Test
    public void testDeviceLockMediator_notInSignInFlow_inSignInFlowIsFalse() {
        DeviceLockMediator deviceLockMediator = new DeviceLockMediator(mDelegate, null,
                mDeviceLockAuthenticatorBridge, mAccountReauthenticationUtils, mActivity, null);

        assertFalse("PropertyModel IN_SIGN_IN_FLOW should be False",
                deviceLockMediator.getModel().get(IN_SIGN_IN_FLOW));
    }

    @Test
    public void
    testCreateDeviceLockOnClick_deviceLockCreatedSuccessfully_callsDelegateOnDeviceLockReady() {
        testOnClick(mAccount, ON_CREATE_DEVICE_LOCK_CLICKED, mSuccessfulDeviceLockCreation,
                /* deviceLockChallengeResult */ null, mSuccessfulAccountReauthenticationChallenge,
                /* deviceLockCreationCalls */ 1,
                /* deviceLockChallengesTriggered */ 0, /* accountReauthenticationsTriggered */ 1,
                /* onDeviceLockReadyCalls */ 1, /* onDeviceLockRefusedCalls */ 0);
    }

    @Test
    public void testCreateDeviceLockOnClick_nullAccount_noReauthenticationTriggered() {
        testOnClick(null, ON_CREATE_DEVICE_LOCK_CLICKED, mSuccessfulDeviceLockCreation,
                /* deviceLockChallengeResult */ null, mSuccessfulAccountReauthenticationChallenge,
                /* deviceLockCreationCalls */ 1,
                /* deviceLockChallengesTriggered */ 0, /* accountReauthenticationsTriggered */ 0,
                /* onDeviceLockReadyCalls */ 1, /* onDeviceLockRefusedCalls */ 0);
    }

    @Test
    public void
    testCreateDeviceLockOnClick_previouslySetDeviceLock_callsDelegateOnDeviceLockReady() {
        doReturn(true).when(mKeyguardManager).isDeviceSecure();
        testOnClick(mAccount, ON_CREATE_DEVICE_LOCK_CLICKED,
                /* deviceLockCreationResult */ null, /* deviceLockChallengeResult */ null,
                mSuccessfulAccountReauthenticationChallenge, /* deviceLockCreationCalls */ 0,
                /* deviceLockChallengesTriggered */ 0, /* accountReauthenticationsTriggered */ 1,
                /* onDeviceLockReadyCalls */ 1, /* onDeviceLockRefusedCalls */ 0);
    }

    @Test
    public void testCreateDeviceLockOnClick_noDeviceLockCreated_noDelegateCalls() {
        testOnClick(mAccount, ON_CREATE_DEVICE_LOCK_CLICKED, mFailedDeviceLockCreation,
                /* deviceLockChallengeResult */ null,
                /* accountReauthenticationResult */ null, /* deviceLockCreationCalls */ 1,
                /* deviceLockChallengesTriggered */ 0, /* accountReauthenticationsTriggered */ 0,
                /* onDeviceLockReadyCalls */ 0, /* onDeviceLockRefusedCalls */ 0);
    }

    @Test
    public void testCreateDeviceLockOnClick_rejectedAccountReauthentication_noDelegateCalls() {
        testOnClick(mAccount, ON_CREATE_DEVICE_LOCK_CLICKED, mSuccessfulDeviceLockCreation,
                /* deviceLockChallengeResult */ null, mRejectedAccountReauthenticationChallenge,
                /* deviceLockCreationCalls */ 1,
                /* deviceLockChallengesTriggered */ 0, /* accountReauthenticationsTriggered */ 1,
                /* onDeviceLockReadyCalls */ 0, /* onDeviceLockRefusedCalls */ 0);
    }

    @Test
    public void
    testGoToOSSettingsOnClick_deviceLockCreatedSuccessfully_callsDelegateOnDeviceLockReady() {
        testOnClick(mAccount, ON_GO_TO_OS_SETTINGS_CLICKED, mSuccessfulDeviceLockCreation,
                /* deviceLockChallengeResult */ null, mSuccessfulAccountReauthenticationChallenge,
                /* deviceLockCreationCalls */ 1,
                /* deviceLockChallengesTriggered */ 0, /* accountReauthenticationsTriggered */ 1,
                /* onDeviceLockReadyCalls */ 1, /* onDeviceLockRefusedCalls */ 0);
    }

    @Test
    public void testGoToOSSettingsOnClick_previouslySetDeviceLock_callsDelegateOnDeviceLockReady() {
        doReturn(true).when(mKeyguardManager).isDeviceSecure();
        testOnClick(mAccount, ON_GO_TO_OS_SETTINGS_CLICKED,
                /* deviceLockCreationResult */ null, /* deviceLockChallengeResult */ null,
                mSuccessfulAccountReauthenticationChallenge, /* deviceLockCreationCalls */ 0,
                /* deviceLockChallengesTriggered */ 0, /* accountReauthenticationsTriggered */ 1,
                /* onDeviceLockReadyCalls */ 1, /* onDeviceLockRefusedCalls */ 0);
    }

    @Test
    public void testGoToOSSettingsOnClick_noDeviceLockCreated_noDelegateCalls() {
        testOnClick(mAccount, ON_GO_TO_OS_SETTINGS_CLICKED, mFailedDeviceLockCreation,
                /* deviceLockChallengeResult */ null,
                /* accountReauthenticationResult */ null, /* deviceLockCreationCalls */ 1,
                /* deviceLockChallengesTriggered */ 0, /* accountReauthenticationsTriggered */ 0,
                /* onDeviceLockReadyCalls */ 0, /* onDeviceLockRefusedCalls */ 0);
    }

    @Test
    public void testGoToOSSettingsOnClick_rejectedAccountReauthentication_noDelegateCalls() {
        testOnClick(mAccount, ON_GO_TO_OS_SETTINGS_CLICKED, mSuccessfulDeviceLockCreation,
                /* deviceLockChallengeResult */ null, mRejectedAccountReauthenticationChallenge,
                /* deviceLockCreationCalls */ 1, /* deviceLockChallengesTriggered */ 0,
                /* accountReauthenticationsTriggered */ 1, /* onDeviceLockReadyCalls */ 0,
                /* onDeviceLockRefusedCalls */ 0);
    }

    @Test
    public void testUserUnderstandsOnClick_successfulChallenges_callsDelegateOnDeviceLockReady() {
        testOnClick(mAccount, ON_USER_UNDERSTANDS_CLICKED,
                /* deviceLockCreationResult */ null, mSuccessfulDeviceLockChallenge,
                mSuccessfulAccountReauthenticationChallenge, /* deviceLockCreationCalls */ 0,
                /* deviceLockChallengesTriggered */ 1, /* accountReauthenticationsTriggered */ 1,
                /* onDeviceLockReadyCalls */ 1, /* onDeviceLockRefusedCalls */ 0);
    }

    @Test
    public void testUserUnderstandsOnClick_failedDeviceLockChallenge_noDelegateCalls() {
        testOnClick(mAccount, ON_USER_UNDERSTANDS_CLICKED,
                /* deviceLockCreationResult */ null, mFailedDeviceLockChallenge,
                /* accountReauthenticationResult */ null, /* deviceLockCreationCalls */ 0,
                /* deviceLockChallengesTriggered */ 1, /* accountReauthenticationsTriggered */ 0,
                /* onDeviceLockReadyCalls */ 0, /* onDeviceLockRefusedCalls */ 0);
    }

    @Test
    public void testUserUnderstandsOnClick_rejectedAccountReauthentication_noDelegateCalls() {
        testOnClick(mAccount, ON_USER_UNDERSTANDS_CLICKED,
                /* deviceLockCreationResult */ null, mSuccessfulDeviceLockChallenge,
                mRejectedAccountReauthenticationChallenge, /* deviceLockCreationCalls */ 0,
                /* deviceLockChallengesTriggered */ 1, /* accountReauthenticationsTriggered */ 1,
                /* onDeviceLockReadyCalls */ 0, /* onDeviceLockRefusedCalls */ 0);
    }

    @Test
    public void testUserUnderstandsOnClick_errorAccountReauthentication_noDelegateCalls() {
        testOnClick(mAccount, ON_USER_UNDERSTANDS_CLICKED,
                /* deviceLockCreationResult */ null, mSuccessfulDeviceLockChallenge,
                mErrorAccountReauthenticationChallenge, /* deviceLockCreationCalls */ 0,
                /* deviceLockChallengesTriggered */ 1, /* accountReauthenticationsTriggered */ 1,
                /* onDeviceLockReadyCalls */ 0, /* onDeviceLockRefusedCalls */ 0);
    }

    @Test
    public void testDeviceLockMediator_dismissOnClick_callsDelegateOnDeviceLockRefused() {
        testOnClick(mAccount, ON_DISMISS_CLICKED,
                /* deviceLockCreationResult */ null, /* deviceLockChallengeResult */ null,
                /* accountReauthenticationResult */ null, /* deviceLockCreationCalls */ 0,
                /* deviceLockChallengesTriggered */ 0, /* accountReauthenticationsTriggered */ 0,
                /* onDeviceLockReadyCalls */ 0, /* onDeviceLockRefusedCalls */ 1);
    }

    private void testOnClick(Account account,
            PropertyModel.ReadableObjectPropertyKey<View.OnClickListener> onClick,
            Answer<Object> deviceLockCreationResult, Answer<Object> deviceLockChallengeResult,
            Answer<Object> accountReauthenticationResult, int deviceLockCreationCalls,
            int deviceLockChallengesTriggered, int accountReauthenticationsTriggered,
            int onDeviceLockReadyCalls, int onDeviceLockRefusedCalls) {
        if (deviceLockCreationResult != null) {
            doAnswer(deviceLockCreationResult)
                    .when(mWindowAndroid)
                    .showIntent(
                            any(Intent.class), any(WindowAndroid.IntentCallback.class), isNull());
        }
        if (deviceLockChallengeResult != null) {
            doAnswer(deviceLockChallengeResult)
                    .when(mDeviceLockAuthenticatorBridge)
                    .reauthenticate(any(), eq(false));
        }
        if (accountReauthenticationResult != null) {
            doAnswer(accountReauthenticationResult)
                    .when(mAccountReauthenticationUtils)
                    .confirmCredentialsOrRecentAuthentication(any(), any(), any(), any());
        }

        DeviceLockMediator deviceLockMediator = new DeviceLockMediator(mDelegate, mWindowAndroid,
                mDeviceLockAuthenticatorBridge, mAccountReauthenticationUtils, mActivity, account);
        deviceLockMediator.getModel().get(onClick).onClick(mMockView);

        verify(mWindowAndroid, times(deviceLockCreationCalls))
                .showIntent(any(Intent.class), any(WindowAndroid.IntentCallback.class), any());
        verify(mDeviceLockAuthenticatorBridge, times(deviceLockChallengesTriggered))
                .reauthenticate(any(), eq(false));
        verify(mAccountReauthenticationUtils, times(accountReauthenticationsTriggered))
                .confirmCredentialsOrRecentAuthentication(any(), any(), any(), any());
        verify(mDelegate, times(onDeviceLockReadyCalls)).onDeviceLockReady();
        verify(mDelegate, times(onDeviceLockRefusedCalls)).onDeviceLockRefused();
    }

    private class MockDelegate implements DeviceLockCoordinator.Delegate {
        @Override
        public void setView(View view) {}

        @Override
        public void onDeviceLockReady() {}

        @Override
        public void onDeviceLockRefused() {}
    }
}
