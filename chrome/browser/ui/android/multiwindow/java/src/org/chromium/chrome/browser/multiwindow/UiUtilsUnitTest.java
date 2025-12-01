// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.app.Dialog;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;

import androidx.annotation.DrawableRes;

import com.google.android.material.textfield.TextInputEditText;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.shadows.ShadowDialog;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.UiUtils.NameWindowDialogSource;
import org.chromium.components.favicon.LargeIconBridge;

/** Tests for {@link UiUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures(ChromeFeatureList.INSTANCE_SWITCHER_V2)
public class UiUtilsUnitTest {
    // Title
    private static final String TITLE = "Title";
    private static final String CUSTOM_TITLE = "Custom Title";
    private static final String EMPTY_WINDOW = "Empty Window";
    private static final String INCOGNITO = "Incognito";
    private static final String INCOGNITO_WINDOW = "Incognito window";

    // Description
    private static final String NO_TABS = "No tabs";
    private static final String TWO_TABS_ONE_INCOGNITO = "2 tabs, 1 incognito";
    private static final String CURRENT = "Current";
    private static final String OPEN = "Window is open";

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock Context mContext;
    @Mock Resources mResources;
    @Mock Drawable mDrawable;
    @Mock LargeIconBridge mIconBridge;

    private final @NameWindowDialogSource int mNameWindowDialogSource =
            NameWindowDialogSource.WINDOW_MANAGER;
    private UiUtils mUiUtils;

    @Before
    public void setUp() {
        doReturn(mResources).when(mContext).getResources();
        doReturn(mDrawable).when(mResources).getDrawable(anyInt(), any());
        doReturn(EMPTY_WINDOW)
                .when(mResources)
                .getString(R.string.instance_switcher_entry_empty_window);
        doReturn(INCOGNITO).when(mResources).getString(R.string.notification_incognito_tab);
        doReturn(INCOGNITO_WINDOW)
                .when(mResources)
                .getString(R.string.instance_switcher_title_incognito_window);
        doReturn(NO_TABS).when(mResources).getString(R.string.instance_switcher_tab_count_zero);
        doReturn(TWO_TABS_ONE_INCOGNITO)
                .when(mResources)
                .getQuantityString(R.plurals.instance_switcher_desc_mixed, 2, 1, 2, 1);
        doReturn(CURRENT).when(mResources).getString(R.string.instance_switcher_current_window);
        doReturn(OPEN).when(mResources).getString(R.string.instance_switcher_adjacent_window);
        mUiUtils =
                new UiUtils(mContext, mIconBridge) {
                    @Override
                    Drawable getTintedIcon(@DrawableRes int drawableId) {
                        return null;
                    }
                };
    }

    @Test
    public void testShowNameWindowDialog_NonEmptyTitle_Success() {
        Activity activity = Robolectric.setupActivity(Activity.class);
        Callback<String> mockCallback = mock(Callback.class);
        String currentTitle = "Window 1";
        String newTitle = "My Window";

        UiUtils.showNameWindowDialog(activity, currentTitle, mockCallback, mNameWindowDialogSource);

        Dialog dialog = ShadowDialog.getLatestDialog();
        assertTrue(dialog.isShowing());

        TextInputEditText editText = dialog.findViewById(R.id.title_input_text);
        editText.setText(newTitle);

        dialog.findViewById(R.id.positive_button).performClick();

        assertFalse(dialog.isShowing());
        verify(mockCallback).onResult(newTitle);
    }

    @Test
    public void testShowNameWindowDialog_EmptyTitle_Success() {
        Activity activity = Robolectric.setupActivity(Activity.class);
        Callback<String> mockCallback = mock(Callback.class);
        String currentTitle = "Window 1";
        String newTitle = "";

        UiUtils.showNameWindowDialog(activity, currentTitle, mockCallback, mNameWindowDialogSource);

        Dialog dialog = ShadowDialog.getLatestDialog();
        assertTrue(dialog.isShowing());

        TextInputEditText editText = dialog.findViewById(R.id.title_input_text);
        editText.setText(newTitle);

        dialog.findViewById(R.id.positive_button).performClick();

        assertFalse(dialog.isShowing());
        verify(mockCallback).onResult(newTitle);
    }

    @Test
    public void testShowNameWindowDialog_SameTitle() {
        Activity activity = Robolectric.setupActivity(Activity.class);
        Callback<String> mockCallback = mock(Callback.class);
        String currentTitle = "Window 1";

        UiUtils.showNameWindowDialog(activity, currentTitle, mockCallback, mNameWindowDialogSource);

        Dialog dialog = ShadowDialog.getLatestDialog();
        assertTrue(dialog.isShowing());

        // EditText is already pre-filled with currentTitle.
        dialog.findViewById(R.id.positive_button).performClick();

        assertFalse(dialog.isShowing());
        verify(mockCallback, never()).onResult(any());
    }

    @Test
    public void testShowNameWindowDialog_Cancel() {
        Activity activity = Robolectric.setupActivity(Activity.class);
        Callback<String> mockCallback = mock(Callback.class);
        String currentTitle = "Window 1";

        UiUtils.showNameWindowDialog(activity, currentTitle, mockCallback, mNameWindowDialogSource);

        Dialog dialog = ShadowDialog.getLatestDialog();
        assertTrue(dialog.isShowing());

        dialog.findViewById(R.id.negative_button).performClick();

        assertFalse(dialog.isShowing());
        verify(mockCallback, never()).onResult(any());
    }

    private void testItemTitle(boolean shouldOpenIncognitoAsWindow) {
        // Normal tabs only with custom title -> custom title
        assertEquals(
                "Instance with normal tabs and custom title has a wrong title",
                CUSTOM_TITLE,
                mUiUtils.getItemTitle(mockInstance(57, 1, 0, false, CUSTOM_TITLE)));

        assertEquals(
                "Instance with normal tabs and custom title has a wrong title",
                TITLE,
                mUiUtils.getItemTitle(mockInstance(57, 1, 0, false, /* customTitle= */ null)));

        // Empty window
        assertEquals(
                "Instance with no tabs has a wrong title",
                EMPTY_WINDOW,
                mUiUtils.getItemTitle(mockInstance(57, 0, 0, false)));

        assertEquals(
                "Instance should be empty if not initialized yet",
                EMPTY_WINDOW,
                mUiUtils.getItemTitle(mockInstanceBeforeLoadingTab(57)));

        // Normal tabs only -> TITLE
        assertEquals(
                "Instance with normal tabs has a wrong title",
                TITLE,
                mUiUtils.getItemTitle(mockInstance(57, 1, 0, false)));

        if (shouldOpenIncognitoAsWindow) {
            // Incognito window -> Incognito window
            assertEquals(
                    "Instance with only incognito tabs has a wrong title",
                    INCOGNITO_WINDOW,
                    mUiUtils.getItemTitle(mockInstance(57, 0, 1, true)));

        } else {
            // Incognito selected -> Incognito, regardless of # of normal tabs
            assertEquals(
                    "Instance with only incognito tabs has a wrong title",
                    INCOGNITO,
                    mUiUtils.getItemTitle(mockInstance(57, 1, 1, true)));
            assertEquals(
                    "Instance with only incognito tabs has a wrong title",
                    INCOGNITO,
                    mUiUtils.getItemTitle(mockInstance(57, 0, 1, true)));

            // Incognito-selected, mixed tabs, killed task -> TITLE (use normal tab info)
            assertEquals(
                    "Incognito-selected, mixed tab, killed task should show normal tab title",
                    TITLE,
                    mUiUtils.getItemTitle(mockInstance(-1, 1, 1, true)));
        }
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testItemTitleWithIncognitoWindow() {
        testItemTitle(/* shouldOpenIncognitoAsWindow= */ true);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testItemTitleWithoutIncognitoWindow() {
        testItemTitle(/* shouldOpenIncognitoAsWindow= */ false);
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.INSTANCE_SWITCHER_V2,
        ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW
    })
    public void testItemDescriptionWithIncognitoWindow() {
        // Empty window -> No tabs
        assertEquals(
                "Instance with no tabs has a wrong description",
                NO_TABS,
                mUiUtils.getItemDesc(mockInstance(57, 0, 0, false)));

        // Normal tabs only -> # of tabs
        int normalTabCount = 3;
        int incognitoTabCount = 0;
        int totalTabCount = 3;
        InstanceInfo item = mockInstance(57, normalTabCount, incognitoTabCount, false);
        mUiUtils.getItemDesc(item);
        verify(mResources)
                .getQuantityString(
                        R.plurals.instance_switcher_tab_count_nonzero,
                        item.tabCount,
                        item.tabCount);
        clearInvocations(mResources);

        // Incognito-selected, incognito tab only -> # incognito tabs
        incognitoTabCount = 4;
        item = mockInstance(57, 0, incognitoTabCount, true);
        mUiUtils.getItemDesc(item);
        verify(mResources)
                .getQuantityString(
                        R.plurals.instance_switcher_tab_count_nonzero,
                        incognitoTabCount,
                        incognitoTabCount);
        clearInvocations(mResources);

        // Disregard incognito tab count for a killed task -> No tabs
        normalTabCount = 0;
        incognitoTabCount = 2;
        item = mockInstance(UiUtils.INVALID_TASK_ID, normalTabCount, incognitoTabCount, false);
        assertEquals(
                "Instance with no tabs has a wrong description",
                NO_TABS,
                mUiUtils.getItemDesc(item));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    @EnableFeatures(ChromeFeatureList.INSTANCE_SWITCHER_V2)
    public void testItemDescriptionWithoutIncognitoWindow() {
        // Empty window -> No tabs
        assertEquals(
                "Instance with no tabs has a wrong description",
                NO_TABS,
                mUiUtils.getItemDesc(mockInstance(57, 0, 0, false)));

        // Normal tabs only -> # of tabs
        int normalTabCount = 3;
        int incognitoTabCount = 0;
        int totalTabCount = 3;
        InstanceInfo item = mockInstance(57, normalTabCount, incognitoTabCount, false);
        mUiUtils.getItemDesc(item);
        verify(mResources)
                .getQuantityString(
                        R.plurals.instance_switcher_tab_count_nonzero,
                        item.tabCount,
                        item.tabCount);
        clearInvocations(mResources);

        // Current instance -> 2 tabs, 1 incognito
        assertEquals(
                "Current instance has a wrong description",
                TWO_TABS_ONE_INCOGNITO,
                mUiUtils.getItemDesc(mockInstance(InstanceInfo.Type.CURRENT)));

        // Other visible instance -> 2 tabs, 1 incognito
        assertEquals(
                "Visible instance has a wrong description",
                TWO_TABS_ONE_INCOGNITO,
                mUiUtils.getItemDesc(mockInstance(InstanceInfo.Type.ADJACENT)));

        // Mixed tabs -> # tabs, # incognito
        normalTabCount = 3;
        incognitoTabCount = 2;
        totalTabCount = 5;
        item = mockInstance(57, normalTabCount, incognitoTabCount, false);
        mUiUtils.getItemDesc(item);
        verify(mResources)
                .getQuantityString(
                        R.plurals.instance_switcher_desc_mixed,
                        totalTabCount,
                        incognitoTabCount,
                        totalTabCount,
                        incognitoTabCount);
        clearInvocations(mResources);

        // Incognito-selected, mixed tabs -> # tabs, # incognito
        normalTabCount = 7;
        incognitoTabCount = 13;
        totalTabCount = 7 + 13;
        item = mockInstance(57, normalTabCount, incognitoTabCount, true);
        mUiUtils.getItemDesc(item);
        verify(mResources)
                .getQuantityString(
                        R.plurals.instance_switcher_desc_mixed,
                        totalTabCount,
                        incognitoTabCount,
                        totalTabCount,
                        incognitoTabCount);
        clearInvocations(mResources);

        // Incognito-selected, incognito tab only -> # incognito tabs
        incognitoTabCount = 4;
        item = mockInstance(57, 0, incognitoTabCount, true);
        mUiUtils.getItemDesc(item);
        verify(mResources)
                .getQuantityString(
                        R.plurals.instance_switcher_desc_incognito,
                        incognitoTabCount,
                        incognitoTabCount);
        clearInvocations(mResources);

        // Disregard incognito tab count for a killed task -> # tabs
        normalTabCount = 3;
        incognitoTabCount = 2;
        item = mockInstance(UiUtils.INVALID_TASK_ID, normalTabCount, incognitoTabCount, false);
        mUiUtils.getItemDesc(item);
        verify(mResources)
                .getQuantityString(
                        R.plurals.instance_switcher_tab_count_nonzero,
                        normalTabCount,
                        normalTabCount);

        clearInvocations(mResources);

        // Incognito-selected, mixed tabs, killed task -> # tabs
        normalTabCount = 2;
        incognitoTabCount = 2;
        item = mockInstance(UiUtils.INVALID_TASK_ID, normalTabCount, incognitoTabCount, true);
        mUiUtils.getItemDesc(item);
        verify(mResources)
                .getQuantityString(
                        R.plurals.instance_switcher_tab_count_nonzero,
                        normalTabCount,
                        normalTabCount);
    }

    @Test
    @DisableFeatures({
        ChromeFeatureList.INSTANCE_SWITCHER_V2,
        ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW
    })
    public void testItemDescriptionWithoutInstanceSwitcherV2() {
        // Empty window -> No tabs
        assertEquals(
                "Instance with no tabs has a wrong description",
                NO_TABS,
                mUiUtils.getItemDesc(mockInstance(57, 0, 0, false)));

        // Current instance -> current
        assertEquals(
                "Current instance has a wrong description",
                CURRENT,
                mUiUtils.getItemDesc(mockInstance(InstanceInfo.Type.CURRENT)));

        // Other visible instance -> 'window is open'
        assertEquals(
                "Visible instance has a wrong description",
                OPEN,
                mUiUtils.getItemDesc(mockInstance(InstanceInfo.Type.ADJACENT)));

        // Normal tabs only -> # of tabs
        int normalTabCount = 3;
        int incognitoTabCount = 0;
        int totalTabCount = 3;
        InstanceInfo item = mockInstance(57, normalTabCount, incognitoTabCount, false);
        mUiUtils.getItemDesc(item);
        verify(mResources)
                .getQuantityString(
                        R.plurals.instance_switcher_tab_count_nonzero,
                        item.tabCount,
                        item.tabCount);
        clearInvocations(mResources);

        // Mixed tabs -> # tabs, # incognito
        normalTabCount = 3;
        incognitoTabCount = 2;
        totalTabCount = 5;
        item = mockInstance(57, normalTabCount, incognitoTabCount, false);
        mUiUtils.getItemDesc(item);
        verify(mResources)
                .getQuantityString(
                        R.plurals.instance_switcher_desc_mixed,
                        totalTabCount,
                        incognitoTabCount,
                        totalTabCount,
                        incognitoTabCount);
        clearInvocations(mResources);

        // Incognito-selected, incognito tab only -> # incognito tabs
        incognitoTabCount = 4;
        item = mockInstance(57, 0, incognitoTabCount, true);
        mUiUtils.getItemDesc(item);
        verify(mResources)
                .getQuantityString(
                        R.plurals.instance_switcher_desc_incognito,
                        incognitoTabCount,
                        incognitoTabCount);
        clearInvocations(mResources);

        // Incognito-selected, mixed tabs -> # tabs, # incognito
        normalTabCount = 7;
        incognitoTabCount = 13;
        totalTabCount = 7 + 13;
        item = mockInstance(57, normalTabCount, incognitoTabCount, true);
        mUiUtils.getItemDesc(item);
        verify(mResources)
                .getQuantityString(
                        R.plurals.instance_switcher_desc_mixed,
                        totalTabCount,
                        incognitoTabCount,
                        totalTabCount,
                        incognitoTabCount);
        clearInvocations(mResources);

        // Disregard incognito tab count for a killed task -> # tabs
        normalTabCount = 3;
        incognitoTabCount = 2;
        item = mockInstance(UiUtils.INVALID_TASK_ID, normalTabCount, incognitoTabCount, false);
        mUiUtils.getItemDesc(item);
        verify(mResources)
                .getQuantityString(
                        R.plurals.instance_switcher_tab_count_nonzero,
                        normalTabCount,
                        normalTabCount);

        clearInvocations(mResources);

        // Incognito-selected, mixed tabs, killed task -> # tabs
        normalTabCount = 2;
        incognitoTabCount = 2;
        item = mockInstance(UiUtils.INVALID_TASK_ID, normalTabCount, incognitoTabCount, true);
        mUiUtils.getItemDesc(item);
        verify(mResources)
                .getQuantityString(
                        R.plurals.instance_switcher_tab_count_nonzero,
                        normalTabCount,
                        normalTabCount);
    }

    @Test
    public void testCloseConfirmationMessage() {
        // Mixed tabs -> TITLE and # more tabs...
        int normalTabCount = 3;
        int incognitoTabCount = 2;
        int totalTabCount = 5;
        InstanceInfo item = mockInstance(57, normalTabCount, incognitoTabCount, false);
        mUiUtils.getConfirmationMessage(item);
        verify(mResources)
                .getQuantityString(
                        R.plurals.instance_switcher_close_confirm_deleted_tabs_many,
                        totalTabCount - 1,
                        TITLE,
                        totalTabCount - 1,
                        TITLE);
        clearInvocations(mResources);

        // Mixed tabs, incognito-selected -> Incognito and # more tabs...
        item = mockInstance(57, normalTabCount, incognitoTabCount, true);
        mUiUtils.getConfirmationMessage(item);
        verify(mResources)
                .getQuantityString(
                        R.plurals.instance_switcher_close_confirm_deleted_incognito_mixed,
                        normalTabCount,
                        incognitoTabCount,
                        normalTabCount,
                        incognitoTabCount);
        clearInvocations(mResources);

        // Incognito tabs only -> # incognito tabs...
        normalTabCount = 0;
        item = mockInstance(57, normalTabCount, incognitoTabCount, true);
        mUiUtils.getConfirmationMessage(item);
        verify(mResources)
                .getQuantityString(
                        R.plurals.instance_switcher_close_confirm_deleted_incognito,
                        incognitoTabCount,
                        incognitoTabCount);
        clearInvocations(mResources);

        // Single tab -> The tab TITLE...
        normalTabCount = 1;
        incognitoTabCount = 0;
        item = mockInstance(57, normalTabCount, incognitoTabCount, false);
        mUiUtils.getConfirmationMessage(item);
        verify(mResources)
                .getString(R.string.instance_switcher_close_confirm_deleted_tabs_one, TITLE);
        clearInvocations(mResources);

        // No tab -> The window...
        normalTabCount = 0;
        incognitoTabCount = 0;
        item = mockInstance(57, normalTabCount, incognitoTabCount, false);
        mUiUtils.getConfirmationMessage(item);
        verify(mResources).getString(R.string.instance_switcher_close_confirm_deleted_tabs_zero);
        clearInvocations(mResources);

        // Incognito-selected, mixed tabs, killed task ->  TITLE and 2 more tabs...
        // Incognito tabs are not restored. Shown with the last focused normal tab info.
        normalTabCount = 3;
        incognitoTabCount = 2;
        totalTabCount = 3; // 2 incognito tabs are discarded.
        item = mockInstance(-1, normalTabCount, incognitoTabCount, true);
        mUiUtils.getConfirmationMessage(item);
        verify(mResources)
                .getQuantityString(
                        R.plurals.instance_switcher_close_confirm_deleted_tabs_many,
                        totalTabCount - 1,
                        TITLE,
                        totalTabCount - 1,
                        TITLE);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.INSTANCE_SWITCHER_V2)
    public void testCloseConfirmationMessageForInstanceSwitcherV2() {
        // Mixed tabs -> TITLE and # other tabs...
        int normalTabCount = 3;
        int incognitoTabCount = 2;
        int totalTabCount = 5;
        InstanceInfo item = mockInstance(57, normalTabCount, incognitoTabCount, false);
        mUiUtils.getConfirmationMessage(item);
        verify(mResources)
                .getQuantityString(
                        R.plurals.instance_switcher_close_confirm_deleted_tabs_many_v2,
                        totalTabCount - 1,
                        TITLE,
                        totalTabCount - 1,
                        TITLE);
        clearInvocations(mResources);

        // Mixed tabs, incognito-selected -> Incognito and # other tabs...
        item = mockInstance(57, normalTabCount, incognitoTabCount, true);
        mUiUtils.getConfirmationMessage(item);
        verify(mResources)
                .getQuantityString(
                        R.plurals.instance_switcher_close_confirm_deleted_incognito_mixed_v2,
                        normalTabCount,
                        incognitoTabCount,
                        normalTabCount,
                        incognitoTabCount);
        clearInvocations(mResources);

        // Incognito-selected, mixed tabs, killed task ->  TITLE and 2 other tabs...
        // Incognito tabs are not restored. Shown with the last focused normal tab info.
        normalTabCount = 3;
        incognitoTabCount = 2;
        totalTabCount = 3; // 2 incognito tabs are discarded.
        item = mockInstance(-1, normalTabCount, incognitoTabCount, true);
        mUiUtils.getConfirmationMessage(item);
        verify(mResources)
                .getQuantityString(
                        R.plurals.instance_switcher_close_confirm_deleted_tabs_many_v2,
                        totalTabCount - 1,
                        TITLE,
                        totalTabCount - 1,
                        TITLE);
    }

    private InstanceInfo mockInstance(
            int taskId, int tabCount, int incognitoTabCount, boolean isIncognito) {
        return new InstanceInfo(
                /* instanceId= */ 1,
                taskId,
                /* type= */ 0,
                "https://url.com",
                TITLE,
                /* customTitle= */ null,
                tabCount,
                incognitoTabCount,
                isIncognito,
                /* lastAccessedTime= */ 0,
                /* closedByUser= */ false);
    }

    private InstanceInfo mockInstance(
            int taskId,
            int tabCount,
            int incognitoTabCount,
            boolean isIncognito,
            String customTitle) {
        return new InstanceInfo(
                /* instanceId= */ 1,
                /* taskId= */ taskId,
                /* type= */ 0,
                "https://url.com",
                TITLE,
                customTitle,
                tabCount,
                incognitoTabCount,
                isIncognito,
                /* lastAccessedTime= */ 0,
                /* closedByUser= */ false);
    }

    private InstanceInfo mockInstance(int type) {
        return new InstanceInfo(
                /* instanceId= */ 1,
                /* taskId= */ 57,
                type,
                "https://url.com",
                TITLE,
                /* customTitle= */ null,
                /* tabCount= */ 1,
                /* incognitoTabCount= */ 1,
                /* isIncognitoSelected= */ true,
                /* lastAccessedTime= */ 0,
                /* closedByUser= */ false);
    }

    private InstanceInfo mockInstanceBeforeLoadingTab(int type) {
        return new InstanceInfo(
                /* instanceId= */ 1,
                /* taskId= */ 57,
                type,
                /* url= */ null,
                /* title= */ null,
                /* customTitle= */ null,
                /* tabCount= */ 1,
                /* incognitoTabCount= */ 0,
                /* isIncognitoSelected= */ false,
                /* lastAccessedTime= */ 0,
                /* closedByUser= */ false);
    }
}
