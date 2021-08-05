// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.favicon.LargeIconBridgeJni;

/**
 * Tests for {@link UiUtils}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class UiUtilsUnitTest {
    // Title
    private static final String TITLE = "Title";
    private static final String EMPTY_WINDOW = "Empty Window";
    private static final String INCOGNITO = "Incognito";

    // Description
    private static final String NO_TABS = "No tabs";
    private static final String CURRENT = "Current";
    private static final String OPEN = "Window is open";

    @Rule
    public JniMocker mocker = new JniMocker();

    @Mock
    LargeIconBridge.Natives mNativeMock;

    @Mock
    Context mContext;
    @Mock
    Resources mResources;
    @Mock
    Drawable mDrawable;
    @Mock
    Profile mProfile;

    UiUtils mUiUtils;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mocker.mock(LargeIconBridgeJni.TEST_HOOKS, mNativeMock);
        doReturn(mResources).when(mContext).getResources();
        doReturn(mDrawable).when(mResources).getDrawable(anyInt(), any());
        doReturn(EMPTY_WINDOW)
                .when(mResources)
                .getString(R.string.instance_switcher_entry_empty_window);
        doReturn(INCOGNITO).when(mResources).getString(R.string.notification_incognito_tab);
        doReturn(NO_TABS).when(mResources).getString(R.string.instance_switcher_tab_count_zero);

        doReturn(CURRENT).when(mResources).getString(R.string.instance_switcher_current_window);
        doReturn(OPEN).when(mResources).getString(R.string.instance_switcher_adjacent_window);
        Profile.setLastUsedProfileForTesting(mProfile);
        mUiUtils = new UiUtils(mContext);
    }

    @Test
    public void testItemTitle() {
        // Empty window
        assertEquals("Instance with no tabs has a wrong title", EMPTY_WINDOW,
                mUiUtils.getItemTitle(mockInstance(57, 0, 0, false)));

        // Normal tabs only -> TITLE
        assertEquals("Instance with normal tabs has a wrong title", TITLE,
                mUiUtils.getItemTitle(mockInstance(57, 1, 0, false)));

        // Incognito selected -> Incognito, regardless of # of normal tabs
        assertEquals("Instance with only incognito tabs has a wrong title", INCOGNITO,
                mUiUtils.getItemTitle(mockInstance(57, 1, 1, true)));
        assertEquals("Instance with only incognito tabs has a wrong title", INCOGNITO,
                mUiUtils.getItemTitle(mockInstance(57, 0, 1, true)));
    }

    @Test
    public void testItemDescription() {
        // Empty window -> No tabs
        assertEquals("Instance with no tabs has a wrong description", NO_TABS,
                mUiUtils.getItemDesc(mockInstance(57, 0, 0, false)));

        // Current instance -> current
        assertEquals("Current instance has a wrong description", CURRENT,
                mUiUtils.getItemDesc(mockInstance(InstanceInfo.Type.CURRENT)));

        // Other visible instance -> 'window is open'
        assertEquals("Visible instance has a wrong description", OPEN,
                mUiUtils.getItemDesc(mockInstance(InstanceInfo.Type.ADJACENT)));

        // Normal tabs only -> # of tabs
        int normalTabCount = 3;
        int incognitoTabCount = 0;
        int totalTabCount = 3;
        InstanceInfo item = mockInstance(57, normalTabCount, incognitoTabCount, false);
        mUiUtils.getItemDesc(item);
        verify(mResources)
                .getQuantityString(R.plurals.instance_switcher_tab_count_nonzero, item.tabCount,
                        item.tabCount);
        clearInvocations(mResources);

        // Mixed tabs -> # tabs, # incognito
        normalTabCount = 3;
        incognitoTabCount = 2;
        totalTabCount = 5;
        item = mockInstance(57, normalTabCount, incognitoTabCount, false);
        mUiUtils.getItemDesc(item);
        verify(mResources)
                .getQuantityString(R.plurals.instance_switcher_desc_mixed, totalTabCount,
                        incognitoTabCount, totalTabCount, incognitoTabCount);
        clearInvocations(mResources);

        // Incognito-selected, incognito tab only -> # incognito tabs
        incognitoTabCount = 4;
        item = mockInstance(57, 0, incognitoTabCount, true);
        mUiUtils.getItemDesc(item);
        verify(mResources)
                .getQuantityString(R.plurals.instance_switcher_desc_incognito, incognitoTabCount,
                        incognitoTabCount);
        clearInvocations(mResources);

        // Incognito-selected, mixed tabs -> # tabs, # incognito
        normalTabCount = 7;
        incognitoTabCount = 13;
        totalTabCount = 7 + 13;
        item = mockInstance(57, normalTabCount, incognitoTabCount, true);
        mUiUtils.getItemDesc(item);
        verify(mResources)
                .getQuantityString(R.plurals.instance_switcher_desc_mixed, totalTabCount,
                        incognitoTabCount, totalTabCount, incognitoTabCount);
        clearInvocations(mResources);

        // Disregard incognito tab count for a killed task -> # tabs
        normalTabCount = 3;
        incognitoTabCount = 2;
        item = mockInstance(UiUtils.INVALID_TASK_ID, normalTabCount, incognitoTabCount, false);
        mUiUtils.getItemDesc(item);
        verify(mResources)
                .getQuantityString(R.plurals.instance_switcher_tab_count_nonzero, normalTabCount,
                        normalTabCount);

        clearInvocations(mResources);
    }

    private InstanceInfo mockInstance(
            int taskId, int tabCount, int incognitoTabCount, boolean isIncognito) {
        return new InstanceInfo(
                1, taskId, 0, "https://url.com", TITLE, tabCount, incognitoTabCount, isIncognito);
    }

    private InstanceInfo mockInstance(int type) {
        return new InstanceInfo(1, 57, type, "https://url.com", TITLE, 1, 1, true);
    }
}
