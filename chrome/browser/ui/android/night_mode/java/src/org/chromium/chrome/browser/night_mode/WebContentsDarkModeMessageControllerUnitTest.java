// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.night_mode;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.notNull;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.res.Resources;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.night_mode.settings.ThemeSettingsFragment;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;

/**
 * Unit tests for {@link WebContentsDarkModeMessageController}.
 *
 * TODO(https://crbug.com/1252868): Add a test case to have message not send because of feature
 * engagement system.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class WebContentsDarkModeMessageControllerUnitTest {
    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Mock
    Activity mActivity;
    @Mock
    SettingsLauncher mSettingsLauncher;
    @Mock
    MessageDispatcher mMessageDispatcher;

    @Mock
    Profile mMockProfile;
    @Mock
    UserPrefs.Natives mMockUserPrefJni;
    @Mock
    PrefService mMockPrefService;
    @Mock
    Resources mResources;

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);

        mJniMocker.mock(UserPrefsJni.TEST_HOOKS, mMockUserPrefJni);
        when(mMockUserPrefJni.get(any())).thenReturn(mMockPrefService);
        when(mMockPrefService.getBoolean(any())).thenReturn(true);
        when(mActivity.getResources()).thenReturn(mResources);
        when(mResources.getString(anyInt())).thenReturn("");

        WebContentsDarkModeMessageController.sIsEnabledForTesting = true;
        Profile.setLastUsedProfileForTesting(mMockProfile);
    }

    @After
    public void tearDown() {
        Profile.setLastUsedProfileForTesting(null);
    }

    @Test
    public void testSendMessage_enabledAndNotClicked() {
        // Successfully send message.
        WebContentsDarkModeMessageController.sendMessageIfAutoDarkEnabled(
                mActivity, mSettingsLauncher, mMessageDispatcher);
        verify(mMessageDispatcher, times(1)).enqueueWindowScopedMessage(any(), eq(false));

        // Message not clicked, so action was not run.
        verify(mSettingsLauncher, times(0))
                .launchSettingsActivity(eq(mActivity), eq(ThemeSettingsFragment.class), notNull());

        // TODO(crbug.com/1252868): Message dismissed and marked as shown as a result.
        WebContentsDarkModeMessageController.onMessageDismissed(DismissReason.UNKNOWN);
    }

    @Test
    public void testSendMessage_enabledAndClicked() {
        // Successfully send message.
        WebContentsDarkModeMessageController.sendMessageIfAutoDarkEnabled(
                mActivity, mSettingsLauncher, mMessageDispatcher);
        verify(mMessageDispatcher, times(1)).enqueueWindowScopedMessage(any(), eq(false));

        // Message clicked, so action was run.
        WebContentsDarkModeMessageController.onPrimaryAction(mActivity, mSettingsLauncher);
        verify(mSettingsLauncher, times(1))
                .launchSettingsActivity(eq(mActivity), eq(ThemeSettingsFragment.class), notNull());

        // TODO(crbug.com/1252868): Message dismissed and marked as shown as a result.
        WebContentsDarkModeMessageController.onMessageDismissed(DismissReason.UNKNOWN);
    }

    @Test
    public void testSendMessage_featureDisabled() {
        // Feature is disabled.
        when(mMockPrefService.getBoolean(any())).thenReturn(false);
        WebContentsDarkModeMessageController.sIsEnabledForTesting = false;

        // Attempt to send message and fail because feature is disabled.
        WebContentsDarkModeMessageController.sendMessageIfAutoDarkEnabled(
                mActivity, mSettingsLauncher, mMessageDispatcher);
        verify(mMessageDispatcher, times(0)).enqueueWindowScopedMessage(any(), eq(false));

        // Message not shown, so action not run.
        verify(mSettingsLauncher, times(0))
                .launchSettingsActivity(eq(mActivity), eq(ThemeSettingsFragment.class), notNull());

        // TODO(crbug.com/1252868): Message not marked as shown.
    }
}
