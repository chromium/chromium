// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.device_lock;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.ui.device_lock.DeviceLockProperties.DEVICE_SUPPORTS_PIN_CREATION_INTENT;
import static org.chromium.chrome.browser.ui.device_lock.DeviceLockProperties.IN_SIGN_IN_FLOW;
import static org.chromium.chrome.browser.ui.device_lock.DeviceLockProperties.ON_CREATE_DEVICE_LOCK_CLICKED;
import static org.chromium.chrome.browser.ui.device_lock.DeviceLockProperties.ON_DISMISS_CLICKED;
import static org.chromium.chrome.browser.ui.device_lock.DeviceLockProperties.ON_GO_TO_OS_SETTINGS_CLICKED;
import static org.chromium.chrome.browser.ui.device_lock.DeviceLockProperties.ON_USER_UNDERSTANDS_CLICKED;
import static org.chromium.chrome.browser.ui.device_lock.DeviceLockProperties.PREEXISTING_DEVICE_LOCK;

import android.app.KeyguardManager;
import android.app.admin.DevicePolicyManager;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.ApplicationInfo;
import android.content.pm.ResolveInfo;

import androidx.constraintlayout.utils.widget.MockView;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowKeyguardManager;
import org.robolectric.shadows.ShadowPackageManager;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;

/** Unit tests for the {@link DeviceLockMediator}.*/
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DeviceLockMediatorUnitTest {
    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Mock
    public Context mContext;
    @Mock
    private MockDelegate mDelegate;

    private ShadowKeyguardManager mShadowKeyguardManager;
    private ShadowPackageManager mShadowPackageManager;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mContext = ContextUtils.getApplicationContext();
        mDelegate = Mockito.mock(MockDelegate.class);

        mShadowKeyguardManager = Shadows.shadowOf(
                (KeyguardManager) mContext.getSystemService(Context.KEYGUARD_SERVICE));
        mShadowPackageManager = Shadows.shadowOf(mContext.getPackageManager());
    }

    @After
    public void tearDown() {}

    @Test
    public void testDeviceLockMediator_deviceSecure_preExistingDeviceLockIsTrue() {
        mShadowKeyguardManager.setIsDeviceSecure(true);
        DeviceLockMediator deviceLockMediator = new DeviceLockMediator(false, mDelegate, mContext);

        assertTrue("PropertyModel PREEXISTING_DEVICE_LOCK should be True",
                deviceLockMediator.getModel().get(PREEXISTING_DEVICE_LOCK));
    }

    @Test
    public void testDeviceLockMediator_deviceNotSecure_preExistingDeviceLockIsFalse() {
        mShadowKeyguardManager.setIsDeviceSecure(false);
        DeviceLockMediator deviceLockMediator = new DeviceLockMediator(false, mDelegate, mContext);

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
        DeviceLockMediator deviceLockMediator = new DeviceLockMediator(false, mDelegate, mContext);

        assertTrue("PropertyModel DEVICE_SUPPORTS_PIN_CREATION_INTENT should be True",
                deviceLockMediator.getModel().get(DEVICE_SUPPORTS_PIN_CREATION_INTENT));
    }

    @Test
    public void
    testDeviceLockMediator_deviceLockCreationIntentNotSupported_deviceSupportsPINIntentIsFalse() {
        DeviceLockMediator deviceLockMediator = new DeviceLockMediator(false, mDelegate, mContext);

        assertFalse("PropertyModel DEVICE_SUPPORTS_PIN_CREATION_INTENT should be False",
                deviceLockMediator.getModel().get(DEVICE_SUPPORTS_PIN_CREATION_INTENT));
    }

    @Test
    public void testDeviceLockMediator_inSignInFlow_inSignInFlowIsTrue() {
        DeviceLockMediator deviceLockMediator = new DeviceLockMediator(true, mDelegate, mContext);

        assertTrue("PropertyModel IN_SIGN_IN_FLOW should be True",
                deviceLockMediator.getModel().get(IN_SIGN_IN_FLOW));
    }

    @Test
    public void testDeviceLockMediator_notInSignInFlow_inSignInFlowIsFalse() {
        DeviceLockMediator deviceLockMediator = new DeviceLockMediator(false, mDelegate, mContext);

        assertFalse("PropertyModel IN_SIGN_IN_FLOW should be False",
                deviceLockMediator.getModel().get(IN_SIGN_IN_FLOW));
    }

    @Test
    public void testDeviceLockMediator_createDeviceLockOnClick_callsDelegateOnDeviceLockReady() {
        DeviceLockMediator deviceLockMediator = new DeviceLockMediator(false, mDelegate, mContext);
        deviceLockMediator.getModel()
                .get(ON_CREATE_DEVICE_LOCK_CLICKED)
                .onClick(new MockView(mContext));

        verify(mDelegate, times(1)).onDeviceLockReady();
        verify(mDelegate, times(0)).onDeviceLockRefused();
    }

    @Test
    public void testDeviceLockMediator_goToOSSettingsOnClick_callsDelegateOnDeviceLockReady() {
        DeviceLockMediator deviceLockMediator = new DeviceLockMediator(false, mDelegate, mContext);
        deviceLockMediator.getModel()
                .get(ON_GO_TO_OS_SETTINGS_CLICKED)
                .onClick(new MockView(mContext));

        verify(mDelegate, times(1)).onDeviceLockReady();
        verify(mDelegate, times(0)).onDeviceLockRefused();
    }

    @Test
    public void testDeviceLockMediator_userUnderstandsOnClick_callsDelegateOnDeviceLockReady() {
        DeviceLockMediator deviceLockMediator = new DeviceLockMediator(false, mDelegate, mContext);
        deviceLockMediator.getModel()
                .get(ON_USER_UNDERSTANDS_CLICKED)
                .onClick(new MockView(mContext));

        verify(mDelegate, times(1)).onDeviceLockReady();
        verify(mDelegate, times(0)).onDeviceLockRefused();
    }

    @Test
    public void testDeviceLockMediator_dismissOnClick_callsDelegateOnDeviceLockRefused() {
        DeviceLockMediator deviceLockMediator = new DeviceLockMediator(false, mDelegate, mContext);
        deviceLockMediator.getModel().get(ON_DISMISS_CLICKED).onClick(new MockView(mContext));

        verify(mDelegate, times(0)).onDeviceLockReady();
        verify(mDelegate, times(1)).onDeviceLockRefused();
    }

    private class MockDelegate implements DeviceLockCoordinator.Delegate {
        @Override
        public void onDeviceLockReady() {}

        @Override
        public void onDeviceLockRefused() {}
    }
}
