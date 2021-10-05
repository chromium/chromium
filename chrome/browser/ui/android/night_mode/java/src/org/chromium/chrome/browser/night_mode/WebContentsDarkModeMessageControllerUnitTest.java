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
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.night_mode.settings.ThemeSettingsFragment;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
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
    Activity mMockActivity;
    @Mock
    Profile mMockProfile;
    @Mock
    SettingsLauncher mMockSettingsLauncher;
    @Mock
    MessageDispatcher mMockMessageDispatcher;

    @Mock
    UserPrefs.Natives mMockUserPrefJni;
    @Mock
    PrefService mMockPrefService;
    @Mock
    Resources mMockResources;
    @Mock
    Tracker mMockTracker;

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);

        mJniMocker.mock(UserPrefsJni.TEST_HOOKS, mMockUserPrefJni);
        when(mMockUserPrefJni.get(eq(mMockProfile))).thenReturn(mMockPrefService);
        when(mMockPrefService.getBoolean(eq(Pref.WEB_KIT_FORCE_DARK_MODE_ENABLED)))
                .thenReturn(true);
        when(mMockActivity.getResources()).thenReturn(mMockResources);
        when(mMockResources.getString(anyInt())).thenReturn("");

        TrackerFactory.setTrackerForTests(mMockTracker);
    }

    @After
    public void tearDown() {
        TrackerFactory.setTrackerForTests(null);
    }

    @Test
    public void testSendMessage_enabledAndNotClicked() {
        // Successfully send message.
        when(mMockTracker.shouldTriggerHelpUI(
                     eq(FeatureConstants.AUTO_DARK_USER_EDUCATION_MESSAGE_FEATURE)))
                .thenReturn(true);
        WebContentsDarkModeMessageController.attemptToSendMessage(
                mMockActivity, mMockProfile, mMockSettingsLauncher, mMockMessageDispatcher);
        verify(mMockTracker, times(1))
                .shouldTriggerHelpUI(eq(FeatureConstants.AUTO_DARK_USER_EDUCATION_MESSAGE_FEATURE));
        verify(mMockMessageDispatcher, times(1)).enqueueWindowScopedMessage(any(), eq(false));

        // Message not clicked, so action was not run.
        verify(mMockSettingsLauncher, times(0))
                .launchSettingsActivity(
                        eq(mMockActivity), eq(ThemeSettingsFragment.class), notNull());

        // Message dismissed and marked as shown as a result.
        WebContentsDarkModeMessageController.onMessageDismissed(
                mMockProfile, DismissReason.UNKNOWN);
        verify(mMockTracker, times(1))
                .dismissed(eq(FeatureConstants.AUTO_DARK_USER_EDUCATION_MESSAGE_FEATURE));
    }

    @Test
    public void testSendMessage_enabledAndClicked() {
        // Successfully send message.
        when(mMockTracker.shouldTriggerHelpUI(
                     eq(FeatureConstants.AUTO_DARK_USER_EDUCATION_MESSAGE_FEATURE)))
                .thenReturn(true);
        WebContentsDarkModeMessageController.attemptToSendMessage(
                mMockActivity, mMockProfile, mMockSettingsLauncher, mMockMessageDispatcher);
        verify(mMockTracker, times(1))
                .shouldTriggerHelpUI(eq(FeatureConstants.AUTO_DARK_USER_EDUCATION_MESSAGE_FEATURE));
        verify(mMockMessageDispatcher, times(1)).enqueueWindowScopedMessage(any(), eq(false));

        // Message clicked, so action was run.
        WebContentsDarkModeMessageController.onPrimaryAction(mMockActivity, mMockSettingsLauncher);
        verify(mMockSettingsLauncher, times(1))
                .launchSettingsActivity(
                        eq(mMockActivity), eq(ThemeSettingsFragment.class), notNull());

        // Message dismissed and marked as shown as a result.
        WebContentsDarkModeMessageController.onMessageDismissed(
                mMockProfile, DismissReason.UNKNOWN);
        verify(mMockTracker, times(1))
                .dismissed(eq(FeatureConstants.AUTO_DARK_USER_EDUCATION_MESSAGE_FEATURE));
    }

    @Test
    public void testSendMessage_featureDisabled() {
        // Feature is disabled.
        when(mMockPrefService.getBoolean(any())).thenReturn(false);
        when(mMockTracker.shouldTriggerHelpUI(
                     eq(FeatureConstants.AUTO_DARK_USER_EDUCATION_MESSAGE_FEATURE)))
                .thenReturn(true);

        // Attempt to send message and fail because feature is disabled.
        WebContentsDarkModeMessageController.attemptToSendMessage(
                mMockActivity, mMockProfile, mMockSettingsLauncher, mMockMessageDispatcher);
        verify(mMockTracker, times(0))
                .shouldTriggerHelpUI(eq(FeatureConstants.AUTO_DARK_USER_EDUCATION_MESSAGE_FEATURE));
        verify(mMockMessageDispatcher, times(0)).enqueueWindowScopedMessage(any(), eq(false));

        // Message not shown, so action not run.
        verify(mMockSettingsLauncher, times(0))
                .launchSettingsActivity(
                        eq(mMockActivity), eq(ThemeSettingsFragment.class), notNull());

        // Message not marked as shown.
        verify(mMockTracker, times(0))
                .dismissed(eq(FeatureConstants.AUTO_DARK_USER_EDUCATION_MESSAGE_FEATURE));
    }

    @Test
    public void testSendMessage_messageShownBefore() {
        // Message has been shown.
        when(mMockTracker.shouldTriggerHelpUI(
                     eq(FeatureConstants.AUTO_DARK_USER_EDUCATION_MESSAGE_FEATURE)))
                .thenReturn(false);

        // Attempt to send message and fail because message has already been shown.
        WebContentsDarkModeMessageController.attemptToSendMessage(
                mMockActivity, mMockProfile, mMockSettingsLauncher, mMockMessageDispatcher);
        verify(mMockTracker, times(1))
                .shouldTriggerHelpUI(eq(FeatureConstants.AUTO_DARK_USER_EDUCATION_MESSAGE_FEATURE));
        verify(mMockMessageDispatcher, times(0)).enqueueWindowScopedMessage(any(), eq(false));

        // Message not shown, so action not run.
        verify(mMockSettingsLauncher, times(0))
                .launchSettingsActivity(
                        eq(mMockActivity), eq(ThemeSettingsFragment.class), notNull());

        // Message not marked as shown.
        verify(mMockTracker, times(0))
                .dismissed(eq(FeatureConstants.AUTO_DARK_USER_EDUCATION_MESSAGE_FEATURE));
    }
}
