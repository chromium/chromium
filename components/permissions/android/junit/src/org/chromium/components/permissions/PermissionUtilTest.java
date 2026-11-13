// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.permissions;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.app.AppOpsManager;
import android.content.Context;
import android.os.Process;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.ui.base.UiAndroidFeatures;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.permissions.ActivityAndroidPermissionDelegate;
import org.chromium.ui.permissions.AndroidPermissionDelegate;

import java.lang.ref.WeakReference;

/** Robolectric unit tests for {@link PermissionUtil}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PermissionUtilTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private WindowAndroid mWindowAndroid;

    private int mPrevTargetSdkVersion;

    @Before
    public void setUp() {
        mPrevTargetSdkVersion =
                ContextUtils.getApplicationContext().getApplicationInfo().targetSdkVersion;
    }

    @After
    public void tearDown() {
        ContextUtils.getApplicationContext().getApplicationInfo().targetSdkVersion =
                mPrevTargetSdkVersion;
    }

    @Test
    @Config(sdk = 36)
    @EnableFeatures(UiAndroidFeatures.CHECK_LOCAL_NETWORK_PERMISSION_REVOKED_COMPAT)
    public void testGetRequiredAndroidPermissions_localNetwork_sdkBelow37() {
        assertArrayEquals(
                new String[0],
                PermissionUtil.getRequiredAndroidPermissionsForContentSetting(
                        ContentSettingsType.LOCAL_NETWORK_ACCESS));
        assertArrayEquals(
                new String[0],
                PermissionUtil.getRequiredAndroidPermissionsForContentSetting(
                        ContentSettingsType.LOCAL_NETWORK));
    }

    @Test
    @Config(sdk = BaseRobolectricTestRunner.MAX_SDK)
    @EnableFeatures(UiAndroidFeatures.CHECK_LOCAL_NETWORK_PERMISSION_REVOKED_COMPAT)
    public void testGetRequiredAndroidPermissions_localNetwork_sdk37_targetSdkBelow37() {
        Context context = ContextUtils.getApplicationContext();
        context.getApplicationInfo().targetSdkVersion = 36;

        String[] expected = new String[] {PermissionUtil.ANDROID_PERMISSION_ACCESS_LOCAL_NETWORK};
        assertArrayEquals(
                expected,
                PermissionUtil.getRequiredAndroidPermissionsForContentSetting(
                        ContentSettingsType.LOCAL_NETWORK_ACCESS));
        assertArrayEquals(
                expected,
                PermissionUtil.getRequiredAndroidPermissionsForContentSetting(
                        ContentSettingsType.LOCAL_NETWORK));
    }

    @Test
    @Config(sdk = BaseRobolectricTestRunner.MAX_SDK)
    @DisableFeatures(UiAndroidFeatures.CHECK_LOCAL_NETWORK_PERMISSION_REVOKED_COMPAT)
    public void testGetRequiredAndroidPermissions_localNetwork_sdk37_featureDisabled() {
        Context context = ContextUtils.getApplicationContext();

        context.getApplicationInfo().targetSdkVersion = 36;
        assertArrayEquals(
                new String[0],
                PermissionUtil.getRequiredAndroidPermissionsForContentSetting(
                        ContentSettingsType.LOCAL_NETWORK_ACCESS));
        assertArrayEquals(
                new String[0],
                PermissionUtil.getRequiredAndroidPermissionsForContentSetting(
                        ContentSettingsType.LOCAL_NETWORK));

        context.getApplicationInfo().targetSdkVersion = 37;
        String[] expected = new String[] {PermissionUtil.ANDROID_PERMISSION_ACCESS_LOCAL_NETWORK};
        assertArrayEquals(
                expected,
                PermissionUtil.getRequiredAndroidPermissionsForContentSetting(
                        ContentSettingsType.LOCAL_NETWORK_ACCESS));
        assertArrayEquals(
                expected,
                PermissionUtil.getRequiredAndroidPermissionsForContentSetting(
                        ContentSettingsType.LOCAL_NETWORK));
    }

    @Test
    @Config(sdk = BaseRobolectricTestRunner.MAX_SDK)
    @EnableFeatures(UiAndroidFeatures.CHECK_LOCAL_NETWORK_PERMISSION_REVOKED_COMPAT)
    public void testLocalNetworkPermission_revokedCompat_sdk37_targetSdkBelow37() {
        Context context = ContextUtils.getApplicationContext();
        context.getApplicationInfo().targetSdkVersion = 36;

        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        AndroidPermissionDelegate permissionDelegate =
                new ActivityAndroidPermissionDelegate(new WeakReference<>(activity));
        when(mWindowAndroid.hasPermission(anyString()))
                .thenAnswer(inv -> permissionDelegate.hasPermission(inv.getArgument(0)));
        when(mWindowAndroid.canRequestPermission(anyString()))
                .thenAnswer(inv -> permissionDelegate.canRequestPermission(inv.getArgument(0)));

        // Simulate REVOKED_COMPAT: PackageManager reports PERMISSION_GRANTED while AppOpsManager
        // reports MODE_IGNORED.
        Shadows.shadowOf(activity)
                .grantPermissions(PermissionUtil.ANDROID_PERMISSION_ACCESS_LOCAL_NETWORK);
        AppOpsManager appOpsManager =
                (AppOpsManager) context.getSystemService(Context.APP_OPS_SERVICE);
        String op =
                AppOpsManager.permissionToOp(
                        PermissionUtil.ANDROID_PERMISSION_ACCESS_LOCAL_NETWORK);
        assertNotNull(op);
        Shadows.shadowOf(appOpsManager)
                .setMode(op, Process.myUid(), context.getPackageName(), AppOpsManager.MODE_IGNORED);

        assertFalse(
                AndroidPermissionRequester.hasRequiredAndroidPermissionsForContentSetting(
                        permissionDelegate, ContentSettingsType.LOCAL_NETWORK_ACCESS));
        assertFalse(
                AndroidPermissionRequester.hasRequiredAndroidPermissionsForContentSetting(
                        permissionDelegate, ContentSettingsType.LOCAL_NETWORK));
        assertFalse(
                PermissionUtil.canRequestSystemPermission(
                        ContentSettingsType.LOCAL_NETWORK_ACCESS, mWindowAndroid));
        assertFalse(
                PermissionUtil.canRequestSystemPermission(
                        ContentSettingsType.LOCAL_NETWORK, mWindowAndroid));

        // Simulate the user re-enabling the permission in Android Settings (MODE_ALLOWED).
        Shadows.shadowOf(appOpsManager)
                .setMode(op, Process.myUid(), context.getPackageName(), AppOpsManager.MODE_ALLOWED);

        assertTrue(
                AndroidPermissionRequester.hasRequiredAndroidPermissionsForContentSetting(
                        permissionDelegate, ContentSettingsType.LOCAL_NETWORK_ACCESS));
        assertTrue(
                AndroidPermissionRequester.hasRequiredAndroidPermissionsForContentSetting(
                        permissionDelegate, ContentSettingsType.LOCAL_NETWORK));
        assertTrue(
                PermissionUtil.canRequestSystemPermission(
                        ContentSettingsType.LOCAL_NETWORK_ACCESS, mWindowAndroid));
        assertTrue(
                PermissionUtil.canRequestSystemPermission(
                        ContentSettingsType.LOCAL_NETWORK, mWindowAndroid));
    }
}
