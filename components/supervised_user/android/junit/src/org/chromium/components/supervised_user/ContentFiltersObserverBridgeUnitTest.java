// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.supervised_user;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.robolectric.Shadows.shadowOf;

import android.content.ContentResolver;
import android.net.Uri;
import android.provider.Settings;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.build.annotations.NullMarked;

/** Unit tests for {@link ContentFiltersObserverBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
@NullMarked
public class ContentFiltersObserverBridgeUnitTest {
    private static final long NATIVE_BRIDGE = 42L;
    private static final String SETTING_NAME =
            SupervisedUserConstants.BROWSER_CONTENT_FILTERS_SETTING_NAME;
    private static final Uri SETTING_URI = Settings.Secure.getUriFor(SETTING_NAME);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ContentFiltersObserverBridge.Natives mNativesMock;

    private ContentResolver mContentResolver;

    @Before
    public void setUp() {
        ContentFiltersObserverBridgeJni.setInstanceForTesting(mNativesMock);
        mContentResolver = ContextUtils.getApplicationContext().getContentResolver();
    }

    @Test
    public void testDestroyBeforeBackgroundTasksRun() {
        Settings.Secure.putInt(mContentResolver, SETTING_NAME, 0);

        ContentFiltersObserverBridge bridge =
                new ContentFiltersObserverBridge(NATIVE_BRIDGE, SETTING_NAME);
        verify(mNativesMock).onChange(NATIVE_BRIDGE, false);
        assertTrue(
                "ContentObserver should not be registered before background task runs",
                shadowOf(mContentResolver).getContentObservers(SETTING_URI).isEmpty());

        Settings.Secure.putInt(mContentResolver, SETTING_NAME, 1);
        bridge.destroy();
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mNativesMock, never()).onChange(NATIVE_BRIDGE, true);
        assertTrue(
                "ContentObserver should end up unregistered after background tasks complete",
                shadowOf(mContentResolver).getContentObservers(SETTING_URI).isEmpty());
    }

    @Test
    public void testSettingFlipBeforeRegistrationReachesNative() {
        Settings.Secure.putInt(mContentResolver, SETTING_NAME, 0);

        ContentFiltersObserverBridge bridge =
                new ContentFiltersObserverBridge(NATIVE_BRIDGE, SETTING_NAME);
        verify(mNativesMock).onChange(NATIVE_BRIDGE, false);
        assertTrue(
                "ContentObserver should not be registered before background task runs",
                shadowOf(mContentResolver).getContentObservers(SETTING_URI).isEmpty());

        // Flip setting before registerContentObserver runs on sBackgroundTaskRunner.
        Settings.Secure.putInt(mContentResolver, SETTING_NAME, 1);
        RobolectricUtil.runAllBackgroundAndUi();

        assertEquals(
                "ContentObserver should be registered after background task runs",
                1,
                shadowOf(mContentResolver).getContentObservers(SETTING_URI).size());
        verify(mNativesMock).onChange(NATIVE_BRIDGE, true);

        bridge.destroy();
        RobolectricUtil.runAllBackgroundAndUi();
        assertTrue(
                "ContentObserver should be unregistered after destroy()",
                shadowOf(mContentResolver).getContentObservers(SETTING_URI).isEmpty());
    }

    @Test
    public void testUnchangedPostRegistrationCheckDoesNotDuplicateOnChange() {
        Settings.Secure.putInt(mContentResolver, SETTING_NAME, 0);

        ContentFiltersObserverBridge bridge =
                new ContentFiltersObserverBridge(NATIVE_BRIDGE, SETTING_NAME);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mNativesMock, times(1)).onChange(NATIVE_BRIDGE, false);

        bridge.destroy();
        RobolectricUtil.runAllBackgroundAndUi();
    }
}
