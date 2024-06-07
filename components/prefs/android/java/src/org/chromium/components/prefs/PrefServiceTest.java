// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// generate_java_test.py

package org.chromium.components.prefs;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;

/** Unit tests for {@link PrefService}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PrefServiceTest {
    private static final String PREF = "42";
    private static final long NATIVE_HANDLE = 117;

    @Rule public JniMocker mocker = new JniMocker();
    @Mock private PrefService.Natives mNativeMock;

    PrefService mPrefService;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mocker.mock(PrefServiceJni.TEST_HOOKS, mNativeMock);
        mPrefService = new PrefService(NATIVE_HANDLE);
    }

    @Test
    public void testGetBoolean() {
        boolean expected = false;

        doReturn(expected).when(mNativeMock).getBoolean(NATIVE_HANDLE, PREF);

        assertEquals(expected, mPrefService.getBoolean(PREF));
    }

    @Test
    public void testSetBoolean() {
        boolean value = true;

        mPrefService.setBoolean(PREF, value);

        verify(mNativeMock).setBoolean(eq(NATIVE_HANDLE), eq(PREF), eq(value));
    }

    @Test
    public void testGetInteger() {
        int expected = 26;

        doReturn(expected).when(mNativeMock).getInteger(NATIVE_HANDLE, PREF);

        assertEquals(expected, mPrefService.getInteger(PREF));
    }

    @Test
    public void testSetInteger() {
        int value = 62;

        mPrefService.setInteger(PREF, value);

        verify(mNativeMock).setInteger(eq(NATIVE_HANDLE), eq(PREF), eq(value));
    }

    @Test
    public void testGetDouble() {
        double expected = 1.23;

        doReturn(expected).when(mNativeMock).getDouble(NATIVE_HANDLE, PREF);

        assertEquals(expected, mPrefService.getDouble(PREF), 0.01f);
    }

    @Test
    public void testSetDouble() {
        double value = 12.34;

        mPrefService.setDouble(PREF, value);

        verify(mNativeMock).setDouble(eq(NATIVE_HANDLE), eq(PREF), eq(value));
    }

    @Test
    public void testGetLong() {
        long expected = 123L;

        doReturn(expected).when(mNativeMock).getLong(NATIVE_HANDLE, PREF);

        assertEquals(expected, mPrefService.getLong(PREF));
    }

    @Test
    public void testSetLong() {
        long value = 123L;

        mPrefService.setLong(PREF, value);

        verify(mNativeMock).setLong(eq(NATIVE_HANDLE), eq(PREF), eq(value));
    }

    @Test
    public void testGetString() {
        String expected = "foo";

        doReturn(expected).when(mNativeMock).getString(NATIVE_HANDLE, PREF);

        assertEquals(expected, mPrefService.getString(PREF));
    }

    @Test
    public void testSetString() {
        String value = "bar";

        mPrefService.setString(PREF, value);

        verify(mNativeMock).setString(eq(NATIVE_HANDLE), eq(PREF), eq(value));
    }

    @Test
    public void testIsManaged() {
        boolean expected = true;

        doReturn(expected).when(mNativeMock).isManagedPreference(NATIVE_HANDLE, PREF);

        assertEquals(expected, mPrefService.isManagedPreference(PREF));
    }

    @Test
    public void testIsDefaultValuePreference() {
        for (boolean expected : new boolean[] {false, true}) {
            doReturn(expected).when(mNativeMock).isDefaultValuePreference(NATIVE_HANDLE, PREF);
            assertEquals(expected, mPrefService.isDefaultValuePreference(PREF));
        }
    }
}
