// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

import static android.Manifest.permission.WRITE_SECURE_SETTINGS;
import static android.content.pm.PackageManager.PERMISSION_DENIED;
import static android.content.pm.PackageManager.PERMISSION_GRANTED;
import static android.provider.Settings.Global.DEVICE_NAME;
import static android.provider.Settings.Global.DEVICE_PROVISIONED;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.ContentResolver;
import android.content.Context;
import android.os.Build;
import android.provider.Settings;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * Unit Tests for CastSettingsManager
 */
@RunWith(BaseRobolectricTestRunner.class)
public class CastSettingsManagerTest {
    private CastSettingsManager mCastSettingsManager;

    @Mock
    private ContentResolver mContentResolver;
    @Mock
    private Context mContext;
    @Mock
    private CastSettingsManager.OnSettingChangedListener mListener;

    @Before
    public void before() {
        mContext = mock(Context.class);
        mContentResolver = mock(ContentResolver.class);
        mListener = mock(CastSettingsManager.OnSettingChangedListener.class);
        ContextUtils.initApplicationContextForTests(mContext);
        when(mContext.getContentResolver()).thenReturn(mContentResolver);
        mCastSettingsManager = CastSettingsManager.createCastSettingsManager(mContext, mListener);
    }

    @After
    public void after() {
        mCastSettingsManager.dispose();
    }

    @Test
    public void testGetDeviceNameReturnsBuildModelWhenNotSet() {
        assertTrue(Settings.Global.putString(mContentResolver, DEVICE_NAME, null));
        assertEquals(Build.MODEL, mCastSettingsManager.getDeviceName());
    }

    @Test
    public void testGetDeviceNameReturnsExpectedName() {
        assertTrue(Settings.Global.putString(mContentResolver, DEVICE_NAME, "fooDevice"));
        assertEquals("fooDevice", mCastSettingsManager.getDeviceName());
    }

    @Test
    public void testIsCastEnabled() {
        assertTrue(Settings.Global.putInt(mContentResolver, DEVICE_PROVISIONED, 0));
        assertFalse(mCastSettingsManager.isCastEnabled());
        assertTrue(Settings.Global.putInt(mContentResolver, DEVICE_PROVISIONED, 1));
        assertTrue(mCastSettingsManager.isCastEnabled());
    }

    @Test
    public void testUpdateGlobalDeviceNameUpdatesDeviceNameWithPermission() {
        when(mContext.checkSelfPermission(WRITE_SECURE_SETTINGS)).thenReturn(PERMISSION_GRANTED);
        assertTrue(CastSettingsManager.updateGlobalDeviceName("newName"));
        assertEquals("newName", mCastSettingsManager.getDeviceName());
    }

    @Test
    public void testUpdateGlobalDeviceNameReturnsFalseWithoutPermission() {
        when(mContext.checkSelfPermission(WRITE_SECURE_SETTINGS)).thenReturn(PERMISSION_DENIED);
        assertFalse(CastSettingsManager.updateGlobalDeviceName("newName2"));
        assertNotEquals("newName2", mCastSettingsManager.getDeviceName());
    }

    @Test
    public void testSettingsListenersRegistered() {
        verify(mContentResolver)
                .registerContentObserver(Settings.Global.getUriFor(DEVICE_NAME), true,
                        mCastSettingsManager.mDeviceNameObserver);
        verify(mContentResolver)
                .registerContentObserver(Settings.Global.getUriFor(DEVICE_PROVISIONED), true,
                        mCastSettingsManager.mIsDeviceProvisionedObserver);
    }

    @Test
    public void testDeviceNameListenerMethodCalledWhenObserverTriggered() {
        mCastSettingsManager.mDeviceNameObserver.onChange(true);
        verify(mListener).onDeviceNameChanged(mCastSettingsManager.getDeviceName());
    }

    @Test
    public void testCastEnabledListenerMethodCalledWhenObserverTriggered() {
        mCastSettingsManager.mIsDeviceProvisionedObserver.onChange(true);
        verify(mListener).onCastEnabledChanged(mCastSettingsManager.isCastEnabled());
    }
}
