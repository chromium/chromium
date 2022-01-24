// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.night_mode;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.notNull;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.res.Resources;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.FeatureList;
import org.chromium.base.FeatureList.TestValues;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.night_mode.WebContentsDarkModeMessageController.AutoDarkClickableSpan;
import org.chromium.chrome.browser.night_mode.settings.ThemeSettingsFragment;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.shadows.ShadowAppCompatResources;

/**
 * Unit tests for {@link WebContentsDarkModeMessageController}.
 *
 * TODO(https://crbug.com/1252868): Add a test case to have message not send because of feature
 * engagement system.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = ShadowAppCompatResources.class)
public class WebContentsDarkModeMessageControllerUnitTest {
    private static final String USER_ED_FEATURE =
            FeatureConstants.AUTO_DARK_USER_EDUCATION_MESSAGE_FEATURE;
    private static final String OPT_OUT_FEATURE = FeatureConstants.AUTO_DARK_OPT_OUT_FEATURE;
    private static final String DISABLED_EVENT = EventConstants.AUTO_DARK_DISABLED_IN_APP_MENU;
    private static final String TEST_URL = "https://example.com";

    private static class FakeModalDialogManager extends ModalDialogManager {
        private PropertyModel mShownDialogModel;

        public FakeModalDialogManager() {
            super(Mockito.mock(Presenter.class), ModalDialogType.TAB);
        }

        @Override
        public void showDialog(PropertyModel model, int dialogType) {
            mShownDialogModel = model;
        }

        @Override
        public void dismissDialog(PropertyModel model, int dismissalCause) {
            model.get(ModalDialogProperties.CONTROLLER).onDismiss(model, dismissalCause);
            mShownDialogModel = null;
        }

        private void clickButton(int buttonType) {
            mShownDialogModel.get(ModalDialogProperties.CONTROLLER)
                    .onClick(mShownDialogModel, buttonType);
        }
    }

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();
    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Mock
    Activity mMockActivity;
    @Mock
    Profile mMockProfile;
    @Mock
    MessageDispatcher mMockMessageDispatcher;
    @Mock
    SettingsLauncher mMockSettingsLauncher;
    @Mock
    HelpAndFeedbackLauncher mMockFeedbackLauncher;

    @Mock
    UserPrefs.Natives mMockUserPrefJni;
    @Mock
    PrefService mMockPrefService;
    @Mock
    Resources mMockResources;
    @Mock
    Tracker mMockTracker;

    FakeModalDialogManager mModalDialogManager;

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);

        mJniMocker.mock(UserPrefsJni.TEST_HOOKS, mMockUserPrefJni);
        when(mMockUserPrefJni.get(eq(mMockProfile))).thenReturn(mMockPrefService);
        when(mMockPrefService.getBoolean(eq(Pref.WEB_KIT_FORCE_DARK_MODE_ENABLED)))
                .thenReturn(true);
        when(mMockActivity.getResources()).thenReturn(mMockResources);
        when(mMockResources.getString(anyInt())).thenReturn("<link></link>");

        when(mMockTracker.shouldTriggerHelpUI(eq(USER_ED_FEATURE))).thenReturn(true);
        when(mMockTracker.shouldTriggerHelpUI(eq(OPT_OUT_FEATURE))).thenReturn(true);

        mModalDialogManager = new FakeModalDialogManager();

        TrackerFactory.setTrackerForTests(mMockTracker);
    }

    @After
    public void tearDown() {
        TrackerFactory.setTrackerForTests(null);
        FeatureList.setTestValues(null);
    }

    @Test
    public void testSendMessage_enabledAndNotClicked() {
        // Successfully send message.
        WebContentsDarkModeMessageController.attemptToSendMessage(
                mMockActivity, mMockProfile, mMockSettingsLauncher, mMockMessageDispatcher);
        verify(mMockTracker, times(1)).shouldTriggerHelpUI(eq(USER_ED_FEATURE));
        verify(mMockMessageDispatcher, times(1)).enqueueWindowScopedMessage(any(), eq(false));

        // Message not clicked, so action was not run.
        verify(mMockSettingsLauncher, never())
                .launchSettingsActivity(
                        eq(mMockActivity), eq(ThemeSettingsFragment.class), notNull());

        // Message dismissed and marked as shown as a result.
        WebContentsDarkModeMessageController.onMessageDismissed(
                mMockProfile, DismissReason.UNKNOWN);
        verify(mMockTracker, times(1)).dismissed(eq(USER_ED_FEATURE));
    }

    @Test
    public void testSendMessage_enabledAndClicked() {
        // Successfully send message.
        WebContentsDarkModeMessageController.attemptToSendMessage(
                mMockActivity, mMockProfile, mMockSettingsLauncher, mMockMessageDispatcher);
        verify(mMockTracker, times(1)).shouldTriggerHelpUI(eq(USER_ED_FEATURE));
        verify(mMockMessageDispatcher, times(1)).enqueueWindowScopedMessage(any(), eq(false));

        // Message clicked, so action was run.
        WebContentsDarkModeMessageController.onPrimaryAction(mMockActivity, mMockSettingsLauncher);
        verify(mMockSettingsLauncher, times(1))
                .launchSettingsActivity(
                        eq(mMockActivity), eq(ThemeSettingsFragment.class), notNull());

        // Message dismissed and marked as shown as a result.
        WebContentsDarkModeMessageController.onMessageDismissed(
                mMockProfile, DismissReason.UNKNOWN);
        verify(mMockTracker, times(1)).dismissed(eq(USER_ED_FEATURE));
    }

    @Test
    public void testSendMessage_featureDisabled() {
        // Feature is disabled.
        when(mMockPrefService.getBoolean(any())).thenReturn(false);

        // Attempt to send message and fail because feature is disabled.
        WebContentsDarkModeMessageController.attemptToSendMessage(
                mMockActivity, mMockProfile, mMockSettingsLauncher, mMockMessageDispatcher);
        verify(mMockTracker, never()).shouldTriggerHelpUI(eq(USER_ED_FEATURE));
        verify(mMockMessageDispatcher, never()).enqueueWindowScopedMessage(any(), eq(false));

        // Message not shown, so action not run.
        verify(mMockSettingsLauncher, never())
                .launchSettingsActivity(
                        eq(mMockActivity), eq(ThemeSettingsFragment.class), notNull());

        // Message not marked as shown.
        verify(mMockTracker, never()).dismissed(eq(USER_ED_FEATURE));
    }

    @Test
    public void testSendMessage_messageShownBefore() {
        // Message has been shown.
        when(mMockTracker.shouldTriggerHelpUI(eq(USER_ED_FEATURE))).thenReturn(false);

        // Attempt to send message and fail because message has already been shown.
        WebContentsDarkModeMessageController.attemptToSendMessage(
                mMockActivity, mMockProfile, mMockSettingsLauncher, mMockMessageDispatcher);
        verify(mMockTracker, times(1)).shouldTriggerHelpUI(eq(USER_ED_FEATURE));
        verify(mMockMessageDispatcher, never()).enqueueWindowScopedMessage(any(), eq(false));

        // Message not shown, so action not run.
        verify(mMockSettingsLauncher, never())
                .launchSettingsActivity(
                        eq(mMockActivity), eq(ThemeSettingsFragment.class), notNull());

        // Message not marked as shown.
        verify(mMockTracker, never()).dismissed(eq(USER_ED_FEATURE));
    }

    @Test
    public void testShowDialog_ShouldNotTrigger() {
        // Feature engagement conditions not met.
        when(mMockTracker.shouldTriggerHelpUI(eq(OPT_OUT_FEATURE))).thenReturn(false);

        // Attempt to send message and fail because feature engagement conditions not met.
        WebContentsDarkModeMessageController.attemptToShowDialog(mMockActivity, mMockProfile,
                TEST_URL, mModalDialogManager, mMockSettingsLauncher, mMockFeedbackLauncher);
        verify(mMockTracker, times(1)).notifyEvent(eq(DISABLED_EVENT));
        Assert.assertNull(
                "Shown dialog model should be null, since we should not trigger the dialog.",
                mModalDialogManager.mShownDialogModel);
    }

    @Test
    public void testShowDialog_ShouldTrigger() {
        // Attempt to send message and succeed because feature engagement conditions met.
        WebContentsDarkModeMessageController.attemptToShowDialog(mMockActivity, mMockProfile,
                TEST_URL, mModalDialogManager, mMockSettingsLauncher, mMockFeedbackLauncher);
        verify(mMockTracker, times(1)).notifyEvent(eq(DISABLED_EVENT));
        Assert.assertNotNull("Shown dialog model should be non-null, since we trigger the dialog.",
                mModalDialogManager.mShownDialogModel);
    }

    @Test
    public void testDialogController_ClickPositiveButton_FeedbackEnabled() {
        // Enable feedback.
        FeatureList.TestValues testValues = new TestValues();
        testValues.addFieldTrialParamOverride(
                ChromeFeatureList.DARKEN_WEBSITES_CHECKBOX_IN_THEMES_SETTING,
                WebContentsDarkModeMessageController.FEEDBACK_DIALOG_PARAM, Boolean.toString(true));
        FeatureList.setTestValues(testValues);

        // Click on positive button.
        WebContentsDarkModeMessageController.attemptToShowDialog(mMockActivity, mMockProfile,
                TEST_URL, mModalDialogManager, mMockSettingsLauncher, mMockFeedbackLauncher);
        mModalDialogManager.clickButton(ButtonType.POSITIVE);
        verify(mMockFeedbackLauncher, times(1))
                .showFeedback(
                        eq(mMockActivity), eq(mMockProfile), eq(TEST_URL), any(), anyInt(), any());

        // Verify dismissal.
        Assert.assertNull("Shown dialog model should be null after clicking the positive button.",
                mModalDialogManager.mShownDialogModel);
        verify(mMockTracker, times(1)).dismissed(eq(OPT_OUT_FEATURE));
    }

    @Test
    public void testDialogController_ClickPositiveButton_FeedbackDisabled() {
        // Disable feedback.
        FeatureList.TestValues testValues = new TestValues();
        testValues.addFieldTrialParamOverride(
                ChromeFeatureList.DARKEN_WEBSITES_CHECKBOX_IN_THEMES_SETTING,
                WebContentsDarkModeMessageController.FEEDBACK_DIALOG_PARAM,
                Boolean.toString(false));
        FeatureList.setTestValues(testValues);

        // Click on positive button.
        WebContentsDarkModeMessageController.attemptToShowDialog(mMockActivity, mMockProfile,
                TEST_URL, mModalDialogManager, mMockSettingsLauncher, mMockFeedbackLauncher);
        mModalDialogManager.clickButton(ButtonType.POSITIVE);
        verify(mMockSettingsLauncher, times(1))
                .launchSettingsActivity(eq(mMockActivity), eq(ThemeSettingsFragment.class), any());

        // Verify dismissal.
        Assert.assertNull("Shown dialog model should be null after clicking the positive button.",
                mModalDialogManager.mShownDialogModel);
        verify(mMockTracker, times(1)).dismissed(eq(OPT_OUT_FEATURE));
    }

    @Test
    public void testDialogController_ClickNegativeButton() {
        // Click on negative button.
        WebContentsDarkModeMessageController.attemptToShowDialog(mMockActivity, mMockProfile,
                TEST_URL, mModalDialogManager, mMockSettingsLauncher, mMockFeedbackLauncher);
        mModalDialogManager.clickButton(ButtonType.NEGATIVE);

        // Verify dismissal.
        Assert.assertNull("Shown dialog model should be null after clicking the negative button.",
                mModalDialogManager.mShownDialogModel);
        verify(mMockTracker, times(1)).dismissed(eq(OPT_OUT_FEATURE));
    }

    @Test
    public void testShowDialog_ClickTitleIcon() {
        // Click on title icon.
        WebContentsDarkModeMessageController.attemptToShowDialog(mMockActivity, mMockProfile,
                TEST_URL, mModalDialogManager, mMockSettingsLauncher, mMockFeedbackLauncher);
        mModalDialogManager.clickButton(ButtonType.TITLE_ICON);

        // Verify not dismissed.
        Assert.assertNotNull("Shown dialog model should be non-null after clicking the title icon.",
                mModalDialogManager.mShownDialogModel);
        verify(mMockTracker, never()).dismissed(eq(OPT_OUT_FEATURE));
    }

    @Test
    public void testClickableSpan_SettingsLink() {
        AutoDarkClickableSpan clickableSpan =
                new AutoDarkClickableSpan(mMockActivity, mMockSettingsLauncher);
        clickableSpan.onClick(null);
        verify(mMockSettingsLauncher, times(1))
                .launchSettingsActivity(eq(mMockActivity), eq(ThemeSettingsFragment.class), any());
    }
}
