// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.device_lock;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.ui.device_lock.DeviceLockProperties.DEVICE_SUPPORTS_PIN_CREATION_INTENT;
import static org.chromium.chrome.browser.ui.device_lock.DeviceLockProperties.IN_SIGN_IN_FLOW;
import static org.chromium.chrome.browser.ui.device_lock.DeviceLockProperties.ON_CREATE_DEVICE_LOCK_CLICKED;
import static org.chromium.chrome.browser.ui.device_lock.DeviceLockProperties.ON_DISMISS_CLICKED;
import static org.chromium.chrome.browser.ui.device_lock.DeviceLockProperties.ON_GO_TO_OS_SETTINGS_CLICKED;
import static org.chromium.chrome.browser.ui.device_lock.DeviceLockProperties.ON_USER_UNDERSTANDS_CLICKED;
import static org.chromium.chrome.browser.ui.device_lock.DeviceLockProperties.PREEXISTING_DEVICE_LOCK;

import android.app.Activity;
import android.app.KeyguardManager;
import android.app.admin.DevicePolicyManager;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.ApplicationInfo;
import android.content.pm.ResolveInfo;
import android.view.View;

import androidx.constraintlayout.utils.widget.MockView;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.mockito.stubbing.Answer;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowKeyguardManager;
import org.robolectric.shadows.ShadowPackageManager;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.device_reauth.ReauthenticatorBridge;
import org.chromium.ui.base.WindowAndroid;

/** Unit tests for the {@link DeviceLockMediator}.*/
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DeviceLockMediatorUnitTest {
    @Mock
    public Context mContext;
    @Mock
    private MockDelegate mDelegate;
    @Mock
    private WindowAndroid mWindowAndroid;
    @Mock
    private ReauthenticatorBridge mDeviceLockAuthenticatorBridge;

    private ShadowKeyguardManager mShadowKeyguardManager;
    private ShadowPackageManager mShadowPackageManager;

    private final Answer<Object> mSuccessfulDeviceLockCreation = (invocation) -> {
        WindowAndroid.IntentCallback callback = invocation.getArgument(1);
        mShadowKeyguardManager.setIsDeviceSecure(true);
        callback.onIntentCompleted(Activity.RESULT_OK, new Intent());
        return null;
    };

    private final Answer<Object> mFailedDeviceLockCreation = (invocation) -> {
        WindowAndroid.IntentCallback callback = invocation.getArgument(1);
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

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mContext = ContextUtils.getApplicationContext();
        mDelegate = Mockito.mock(MockDelegate.class);

        mShadowKeyguardManager = Shadows.shadowOf(
                (KeyguardManager) mContext.getSystemService(Context.KEYGUARD_SERVICE));
        mShadowPackageManager = Shadows.shadowOf(mContext.getPackageManager());
    }

    @Test
    public void testDeviceLockMediator_deviceSecure_preExistingDeviceLockIsTrue() {
        mShadowKeyguardManager.setIsDeviceSecure(true);
        DeviceLockMediator deviceLockMediator = new DeviceLockMediator(
                false, mDelegate, null, mDeviceLockAuthenticatorBridge, mContext);

        assertTrue("PropertyModel PREEXISTING_DEVICE_LOCK should be True",
                deviceLockMediator.getModel().get(PREEXISTING_DEVICE_LOCK));
    }

    @Test
    public void testDeviceLockMediator_deviceNotSecure_preExistingDeviceLockIsFalse() {
        mShadowKeyguardManager.setIsDeviceSecure(false);

        DeviceLockMediator deviceLockMediator = new DeviceLockMediator(
                false, mDelegate, null, mDeviceLockAuthenticatorBridge, mContext);

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

        mShadowPackageManager.addResolveInfoForIntent(intent, resolveInfo);
        DeviceLockMediator deviceLockMediator = new DeviceLockMediator(
                false, mDelegate, null, mDeviceLockAuthenticatorBridge, mContext);

        assertTrue("PropertyModel DEVICE_SUPPORTS_PIN_CREATION_INTENT should be True",
                deviceLockMediator.getModel().get(DEVICE_SUPPORTS_PIN_CREATION_INTENT));
    }

    @Test
    public void
    testDeviceLockMediator_deviceLockCreationIntentNotSupported_deviceSupportsPINIntentIsFalse() {
        DeviceLockMediator deviceLockMediator = new DeviceLockMediator(
                false, mDelegate, null, mDeviceLockAuthenticatorBridge, mContext);

        assertFalse("PropertyModel DEVICE_SUPPORTS_PIN_CREATION_INTENT should be False",
                deviceLockMediator.getModel().get(DEVICE_SUPPORTS_PIN_CREATION_INTENT));
    }

    @Test
    public void testDeviceLockMediator_inSignInFlow_inSignInFlowIsTrue() {
        DeviceLockMediator deviceLockMediator = new DeviceLockMediator(
                true, mDelegate, null, mDeviceLockAuthenticatorBridge, mContext);

        assertTrue("PropertyModel IN_SIGN_IN_FLOW should be True",
                deviceLockMediator.getModel().get(IN_SIGN_IN_FLOW));
    }

    @Test
    public void testDeviceLockMediator_notInSignInFlow_inSignInFlowIsFalse() {
        DeviceLockMediator deviceLockMediator = new DeviceLockMediator(
                false, mDelegate, null, mDeviceLockAuthenticatorBridge, mContext);

        assertFalse("PropertyModel IN_SIGN_IN_FLOW should be False",
                deviceLockMediator.getModel().get(IN_SIGN_IN_FLOW));
    }

    @Test
    public void
    testCreateDeviceLockOnClick_deviceLockCreatedSuccessfully_callsDelegateOnDeviceLockReady() {
        doAnswer(mSuccessfulDeviceLockCreation)
                .when(mWindowAndroid)
                .showIntent(any(Intent.class), any(WindowAndroid.IntentCallback.class), isNull());

        DeviceLockMediator deviceLockMediator = new DeviceLockMediator(
                false, mDelegate, mWindowAndroid, mDeviceLockAuthenticatorBridge, mContext);

        deviceLockMediator.getModel()
                .get(ON_CREATE_DEVICE_LOCK_CLICKED)
                .onClick(new MockView(mContext));

        verify(mWindowAndroid, times(1))
                .showIntent(any(Intent.class), any(WindowAndroid.IntentCallback.class), any());
        verify(mDelegate, times(1)).onDeviceLockReady();
        verify(mDelegate, times(0)).onDeviceLockRefused();
    }

    @Test
    public void testCreateDeviceLockOnClick_noDeviceLockCreated_noDelegateCalls() {
        doAnswer(mFailedDeviceLockCreation)
                .when(mWindowAndroid)
                .showIntent(any(Intent.class), any(WindowAndroid.IntentCallback.class), isNull());

        DeviceLockMediator deviceLockMediator = new DeviceLockMediator(
                false, mDelegate, mWindowAndroid, mDeviceLockAuthenticatorBridge, mContext);

        deviceLockMediator.getModel()
                .get(ON_CREATE_DEVICE_LOCK_CLICKED)
                .onClick(new MockView(mContext));

        verify(mWindowAndroid, times(1))
                .showIntent(any(Intent.class), any(WindowAndroid.IntentCallback.class), any());
        verify(mDelegate, times(0)).onDeviceLockReady();
        verify(mDelegate, times(0)).onDeviceLockRefused();
    }

    @Test
    public void
    testGoToOSSettingsOnClick_deviceLockCreatedSuccessfully_callsDelegateOnDeviceLockReady() {
        doAnswer(mSuccessfulDeviceLockCreation)
                .when(mWindowAndroid)
                .showIntent(any(Intent.class), any(WindowAndroid.IntentCallback.class), isNull());

        DeviceLockMediator deviceLockMediator = new DeviceLockMediator(
                false, mDelegate, mWindowAndroid, mDeviceLockAuthenticatorBridge, mContext);

        deviceLockMediator.getModel()
                .get(ON_GO_TO_OS_SETTINGS_CLICKED)
                .onClick(new MockView(mContext));

        verify(mWindowAndroid, times(1))
                .showIntent(any(Intent.class), any(WindowAndroid.IntentCallback.class), any());
        verify(mDelegate, times(1)).onDeviceLockReady();
        verify(mDelegate, times(0)).onDeviceLockRefused();
    }

    @Test
    public void testGoToOSSettingsOnClick_noDeviceLockCreated_noDelegateCalls() {
        doAnswer(mFailedDeviceLockCreation)
                .when(mWindowAndroid)
                .showIntent(any(Intent.class), any(WindowAndroid.IntentCallback.class), isNull());

        DeviceLockMediator deviceLockMediator = new DeviceLockMediator(
                false, mDelegate, mWindowAndroid, mDeviceLockAuthenticatorBridge, mContext);

        deviceLockMediator.getModel()
                .get(ON_GO_TO_OS_SETTINGS_CLICKED)
                .onClick(new MockView(mContext));

        verify(mWindowAndroid, times(1))
                .showIntent(any(Intent.class), any(WindowAndroid.IntentCallback.class), any());
        verify(mDelegate, times(0)).onDeviceLockReady();
        verify(mDelegate, times(0)).onDeviceLockRefused();
    }

    @Test
    public void
    testUserUnderstandsOnClick_successfulDeviceLockChallenge_callsDelegateOnDeviceLockReady() {
        doAnswer(mSuccessfulDeviceLockChallenge)
                .when(mDeviceLockAuthenticatorBridge)
                .reauthenticate(any(), eq(false));

        DeviceLockMediator deviceLockMediator = new DeviceLockMediator(
                false, mDelegate, null, mDeviceLockAuthenticatorBridge, mContext);

        deviceLockMediator.getModel()
                .get(ON_USER_UNDERSTANDS_CLICKED)
                .onClick(new MockView(mContext));

        verify(mWindowAndroid, times(0))
                .showIntent(any(Intent.class), any(WindowAndroid.IntentCallback.class), any());
        verify(mDeviceLockAuthenticatorBridge, times(1)).reauthenticate(any(), eq(false));
        verify(mDelegate, times(1)).onDeviceLockReady();
        verify(mDelegate, times(0)).onDeviceLockRefused();
    }

    @Test
    public void testUserUnderstandsOnClick_failedDeviceLockChallenge_noDelegateCalls() {
        doAnswer(mFailedDeviceLockChallenge)
                .when(mDeviceLockAuthenticatorBridge)
                .reauthenticate(any(), eq(false));

        DeviceLockMediator deviceLockMediator = new DeviceLockMediator(
                false, mDelegate, null, mDeviceLockAuthenticatorBridge, mContext);

        deviceLockMediator.getModel()
                .get(ON_USER_UNDERSTANDS_CLICKED)
                .onClick(new MockView(mContext));

        verify(mWindowAndroid, times(0))
                .showIntent(any(Intent.class), any(WindowAndroid.IntentCallback.class), any());
        verify(mDeviceLockAuthenticatorBridge, times(1)).reauthenticate(any(), eq(false));
        verify(mDelegate, times(0)).onDeviceLockReady();
        verify(mDelegate, times(0)).onDeviceLockRefused();
    }

    @Test
    public void testDeviceLockMediator_dismissOnClick_callsDelegateOnDeviceLockRefused() {
        DeviceLockMediator deviceLockMediator = new DeviceLockMediator(
                false, mDelegate, null, mDeviceLockAuthenticatorBridge, mContext);

        deviceLockMediator.getModel().get(ON_DISMISS_CLICKED).onClick(new MockView(mContext));

        verify(mDelegate, times(0)).onDeviceLockReady();
        verify(mDelegate, times(1)).onDeviceLockRefused();
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
