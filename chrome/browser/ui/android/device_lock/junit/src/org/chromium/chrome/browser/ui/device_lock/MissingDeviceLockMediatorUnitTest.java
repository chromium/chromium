// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.device_lock;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;

import static org.chromium.chrome.browser.ui.device_lock.MissingDeviceLockProperties.ON_CHECKBOX_TOGGLED;
import static org.chromium.chrome.browser.ui.device_lock.MissingDeviceLockProperties.ON_CONTINUE_CLICKED;
import static org.chromium.chrome.browser.ui.device_lock.MissingDeviceLockProperties.ON_CREATE_DEVICE_LOCK_CLICKED;
import static org.chromium.chrome.browser.ui.device_lock.MissingDeviceLockProperties.REMOVE_ALL_LOCAL_DATA_CHECKED;

import android.app.Activity;
import android.app.admin.DevicePolicyManager;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.provider.Settings;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;

/** Unit tests for the {@link DeviceLockMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MissingDeviceLockMediatorUnitTest {
    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    @Mock public Activity mActivity;
    @Mock private PackageManager mPackageManager;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivity = Mockito.mock(Activity.class);
        mPackageManager = Mockito.mock(PackageManager.class);
        doReturn(mPackageManager).when(mActivity).getPackageManager();
    }

    @Test
    public void
            testMissingDeviceLockMediator_deviceLockCreationIntentSupported_deviceSupportsPINIntentIsTrue() {
        ResolveInfo resolveInfo = new ResolveInfo();
        resolveInfo.isDefault = true;

        ApplicationInfo applicationInfo = new ApplicationInfo();
        applicationInfo.packageName = "example.package";
        resolveInfo.activityInfo = new ActivityInfo();
        resolveInfo.activityInfo.applicationInfo = applicationInfo;
        resolveInfo.activityInfo.name = "ExamplePackage";
        doReturn(resolveInfo).when(mPackageManager).resolveActivity(any(), anyInt());

        MissingDeviceLockMediator missingDeviceLockMediator =
                new MissingDeviceLockMediator((wipeAllData) -> {}, mActivity);
        doAnswer(
                        (invocation) -> {
                            Intent intent = invocation.getArgument(0);
                            assertEquals(
                                    intent.getAction(),
                                    DevicePolicyManager.ACTION_SET_NEW_PASSWORD);
                            return null;
                        })
                .when(mActivity)
                .startActivity(any());
        missingDeviceLockMediator.getModel().get(ON_CREATE_DEVICE_LOCK_CLICKED).onClick(null);
    }

    @Test
    public void
            testMissingDeviceLockMediator_pinCreationIntentNotSupported_createDeviceLockThroughSettings() {
        MissingDeviceLockMediator missingDeviceLockMediator =
                new MissingDeviceLockMediator((wipeAllData) -> {}, mActivity);
        doAnswer(
                        (invocation) -> {
                            Intent intent = invocation.getArgument(0);
                            assertEquals(intent.getAction(), Settings.ACTION_SECURITY_SETTINGS);
                            return null;
                        })
                .when(mActivity)
                .startActivity(any());
        missingDeviceLockMediator.getModel().get(ON_CREATE_DEVICE_LOCK_CLICKED).onClick(null);
    }

    @Test
    public void testMissingDeviceLockMediator_continueWithoutDeviceLock_checkboxToggledOn() {
        Callback<Boolean> continueWithoutDeviceLock =
                (wipeAllData) -> {
                    assertTrue(
                            "The input should be true when the checkbox is toggled on.",
                            wipeAllData);
                };
        MissingDeviceLockMediator missingDeviceLockMediator =
                new MissingDeviceLockMediator(continueWithoutDeviceLock, mActivity);

        missingDeviceLockMediator.getModel().set(REMOVE_ALL_LOCAL_DATA_CHECKED, true);
        missingDeviceLockMediator.getModel().get(ON_CONTINUE_CLICKED).onClick(null);
    }

    @Test
    public void testMissingDeviceLockMediator_continueWithoutDeviceLock_checkboxToggledOff() {
        Callback<Boolean> continueWithoutDeviceLock =
                (wipeAllData) -> {
                    assertFalse(
                            "The input should be true when the checkbox is toggled off.",
                            wipeAllData);
                };
        MissingDeviceLockMediator missingDeviceLockMediator =
                new MissingDeviceLockMediator(continueWithoutDeviceLock, mActivity);

        missingDeviceLockMediator.getModel().set(REMOVE_ALL_LOCAL_DATA_CHECKED, false);
        missingDeviceLockMediator.getModel().get(ON_CONTINUE_CLICKED).onClick(null);
    }

    @Test
    public void testMissingDeviceLockMediator_onCheckboxToggled() {
        MissingDeviceLockMediator missingDeviceLockMediator =
                new MissingDeviceLockMediator((wipeAllData) -> {}, mActivity);

        missingDeviceLockMediator.getModel().get(ON_CHECKBOX_TOGGLED).onCheckedChanged(null, true);
        assertTrue(
                "The checkbox should be toggled on.",
                missingDeviceLockMediator.getModel().get(REMOVE_ALL_LOCAL_DATA_CHECKED));
        missingDeviceLockMediator.getModel().get(ON_CHECKBOX_TOGGLED).onCheckedChanged(null, false);
        assertFalse(
                "The checkbox should be toggled off.",
                missingDeviceLockMediator.getModel().get(REMOVE_ALL_LOCAL_DATA_CHECKED));
    }
}
