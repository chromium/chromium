// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.night_mode;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.notNull;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.Callback;
import org.chromium.base.FeatureList;
import org.chromium.base.FeatureList.TestValues;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncher;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.night_mode.WebContentsDarkModeMessageController.AutoDarkClickableSpan;
import org.chromium.chrome.browser.night_mode.WebContentsDarkModeMessageControllerUnitTest.ShadowWebContentsDarkModeController;
import org.chromium.chrome.browser.night_mode.settings.ThemeSettingsFragment;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.shadows.ShadowAppCompatResources;

/**
 * Unit tests for {@link WebContentsDarkModeMessageController}.
 *
 * <p>TODO(crbug.com/40198953): Add a test case to have message not send because of feature
 * engagement system.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowAppCompatResources.class, ShadowWebContentsDarkModeController.class})
public class WebContentsDarkModeMessageControllerUnitTest {
    private static final String USER_ED_FEATURE =
            FeatureConstants.AUTO_DARK_USER_EDUCATION_MESSAGE_FEATURE;
    private static final String USER_ED_OPT_IN_FEATURE =
            FeatureConstants.AUTO_DARK_USER_EDUCATION_MESSAGE_OPT_IN_FEATURE;
    private static final String OPT_OUT_FEATURE = FeatureConstants.AUTO_DARK_OPT_OUT_FEATURE;
    private static final String DISABLED_EVENT = EventConstants.AUTO_DARK_DISABLED_IN_APP_MENU;
    private static final String TEST_URL = "https://example.com";
    private static final String TEST_LINK_STRING = "<link></link>";
    private static final String TEST_OPT_OUT_TITLE = "opt_out";
    private static final String TEST_OPT_IN_TITLE = "opt_in";

    private static class FakeMessageDispatcher implements MessageDispatcher {
        private PropertyModel mShownMessageModel;

        @Override
        public void enqueueMessage(
                PropertyModel messageProperties,
                WebContents webContents,
                int scopeType,
                boolean highPriority) {
            // Not called in this test.
        }

        @Override
        public void enqueueWindowScopedMessage(
                PropertyModel messageProperties, boolean highPriority) {
            mShownMessageModel = messageProperties;
        }

        @Override
        public void dismissMessage(PropertyModel messageProperties, int dismissReason) {
            Callback<Integer> callback =
                    mShownMessageModel.get(MessageBannerProperties.ON_DISMISSED);
            mShownMessageModel = null;
            callback.onResult(dismissReason);
        }

        private void clickButton() {
            mShownMessageModel.get(MessageBannerProperties.ON_PRIMARY_ACTION).get();
        }
    }

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
            mShownDialogModel
                    .get(ModalDialogProperties.CONTROLLER)
                    .onClick(mShownDialogModel, buttonType);
        }
    }

    @Implements(WebContentsDarkModeController.class)
    static class ShadowWebContentsDarkModeController {
        static boolean sIsFeatureEnabled;

        @Implementation
        public static void setGlobalUserSettings(
                BrowserContextHandle browserContextHandle, boolean enabled) {
            sIsFeatureEnabled = enabled;
        }

        @Implementation
        public static boolean isFeatureEnabled(
                Context context, BrowserContextHandle browserContextHandle) {
            return sIsFeatureEnabled;
        }
    }

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock Activity mMockActivity;
    @Mock Profile mMockProfile;
    @Mock WebContents mMockWebContents;
    @Mock SettingsNavigation mMockSettingsNavigation;
    @Mock HelpAndFeedbackLauncher mMockFeedbackLauncher;

    @Mock Resources mMockResources;
    @Mock Tracker mMockTracker;

    FakeMessageDispatcher mMessageDispatcher;
    FakeModalDialogManager mModalDialogManager;

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);

        when(mMockActivity.getResources()).thenReturn(mMockResources);
        when(mMockResources.getString(anyInt())).thenReturn(TEST_LINK_STRING);
        when(mMockResources.getString(eq(R.string.auto_dark_message_title)))
                .thenReturn(TEST_OPT_OUT_TITLE);
        when(mMockResources.getString(eq(R.string.auto_dark_message_opt_in_title)))
                .thenReturn(TEST_OPT_IN_TITLE);

        when(mMockTracker.shouldTriggerHelpUI(eq(USER_ED_FEATURE))).thenReturn(true);
        when(mMockTracker.shouldTriggerHelpUI(eq(USER_ED_OPT_IN_FEATURE))).thenReturn(true);
        when(mMockTracker.shouldTriggerHelpUI(eq(OPT_OUT_FEATURE))).thenReturn(true);

        mMessageDispatcher = new FakeMessageDispatcher();
        mModalDialogManager = new FakeModalDialogManager();

        SettingsNavigationFactory.setInstanceForTesting(mMockSettingsNavigation);
        HelpAndFeedbackLauncherFactory.setInstanceForTesting(mMockFeedbackLauncher);
        TrackerFactory.setTrackerForTests(mMockTracker);
        ShadowWebContentsDarkModeController.sIsFeatureEnabled = true;
    }

    @After
    public void tearDown() {
        FeatureList.setTestValues(null);
    }

    private void setOptOut(boolean optOut) {
        ShadowWebContentsDarkModeController.sIsFeatureEnabled = optOut;
        if (!optOut) {
            FeatureList.TestValues testValues = new TestValues();
            testValues.addFieldTrialParamOverride(
                    ChromeFeatureList.DARKEN_WEBSITES_CHECKBOX_IN_THEMES_SETTING,
                    WebContentsDarkModeMessageController.OPT_OUT_PARAM,
                    Boolean.toString(optOut));
            FeatureList.setTestValues(testValues);
        }
    }

    private void doTestSendMessage_OptOut_Sent(boolean clicked) {
        // Set state based on opt-in/out arm. Since message is sent, shouldTriggerHelpUI is true.
        reset(mMockWebContents, mMockSettingsNavigation, mMockTracker);
        setOptOut(true);
        String enabledFeature = USER_ED_FEATURE;
        String messageTitle = TEST_OPT_OUT_TITLE;
        when(mMockTracker.shouldTriggerHelpUI(eq(enabledFeature))).thenReturn(true);

        // Successfully send message.
        WebContentsDarkModeMessageController.attemptToSendMessage(
                mMockActivity, mMockProfile, mMockWebContents, mMessageDispatcher);
        verify(mMockTracker, times(1)).shouldTriggerHelpUI(enabledFeature);
        Assert.assertNotNull("Message should be non-null.", mMessageDispatcher.mShownMessageModel);
        Assert.assertEquals(
                "Message has incorrect title.",
                mMessageDispatcher.mShownMessageModel.get(MessageBannerProperties.TITLE),
                messageTitle);

        // Message maybe clicked.
        if (clicked) mMessageDispatcher.clickButton();
        verifyLaunchSettings(clicked ? 1 : 0);

        // Message dismissed and marked as shown as a result.
        mMessageDispatcher.dismissMessage(null, DismissReason.UNKNOWN);
        Assert.assertNull(
                "Shown message should be null, since we dismiss the message.",
                mMessageDispatcher.mShownMessageModel);
        verify(mMockTracker, times(1)).dismissed(eq(enabledFeature));
    }

    private void doTestSendMessage_OptIn_Sent(boolean clicked) {
        // Set state based on opt-in/out arm. Since message is sent, shouldTriggerHelpUI is true.
        reset(mMockWebContents, mMockSettingsNavigation, mMockTracker);
        setOptOut(false);
        String enabledFeature = USER_ED_OPT_IN_FEATURE;
        String messageTitle = TEST_OPT_IN_TITLE;
        when(mMockTracker.shouldTriggerHelpUI(eq(enabledFeature))).thenReturn(true);

        // Successfully send message.
        WebContentsDarkModeMessageController.attemptToSendMessage(
                mMockActivity, mMockProfile, mMockWebContents, mMessageDispatcher);
        verify(mMockTracker, times(1)).shouldTriggerHelpUI(enabledFeature);
        Assert.assertNotNull("Message should be non-null.", mMessageDispatcher.mShownMessageModel);
        Assert.assertEquals(
                "Message has incorrect title.",
                mMessageDispatcher.mShownMessageModel.get(MessageBannerProperties.TITLE),
                messageTitle);

        // Message maybe clicked.
        if (clicked) mMessageDispatcher.clickButton();
        Assert.assertEquals(
                "Feature should be enabled if we click the opt-in action.",
                ShadowWebContentsDarkModeController.sIsFeatureEnabled,
                clicked);
        verify(mMockWebContents, times(clicked ? 1 : 0)).notifyRendererPreferenceUpdate();

        // Message dismissed and marked as shown as a result.
        if (clicked) {
            mMessageDispatcher.dismissMessage(null, DismissReason.PRIMARY_ACTION);
            Assert.assertNotNull(
                    "Shown message should be non-null after clicking opt-in action.",
                    mMessageDispatcher.mShownMessageModel);
        } else {
            mMessageDispatcher.dismissMessage(null, DismissReason.UNKNOWN);
            Assert.assertNull(
                    "Shown message should be null, since we dismiss the message.",
                    mMessageDispatcher.mShownMessageModel);
        }
        verify(mMockTracker, times(1)).dismissed(eq(enabledFeature));
    }

    private void doTestSendMessage_NotSent(boolean optOut, boolean enabled, boolean shouldTrigger) {
        // Message will not send if the feature is enabled on the opt-in arm, the feature is
        // disabled on the opt-out arm, or the feature engagement system requirements are not met.
        Assert.assertFalse(
                "Message would send under these params", optOut == enabled && shouldTrigger);

        // Set setting and feature engagement state.
        setOptOut(optOut);
        ShadowWebContentsDarkModeController.sIsFeatureEnabled = enabled;
        String enabledFeature = optOut ? USER_ED_FEATURE : USER_ED_OPT_IN_FEATURE;
        when(mMockTracker.shouldTriggerHelpUI(eq(enabledFeature))).thenReturn(shouldTrigger);

        // Attempt to send message and fail.
        WebContentsDarkModeMessageController.attemptToSendMessage(
                mMockActivity, mMockProfile, mMockWebContents, mMessageDispatcher);
        Assert.assertNull(
                "Shown message should be null, since we don't show the message.",
                mMessageDispatcher.mShownMessageModel);
        verify(mMockTracker, times(optOut == enabled ? 1 : 0))
                .shouldTriggerHelpUI(eq(enabledFeature));

        // Message not shown, so action not run.
        verifyLaunchSettings(0);

        // Message not marked as shown.
        verify(mMockTracker, never()).dismissed(eq(USER_ED_FEATURE));
    }

    private void verifyLaunchSettings(int numTimes) {
        verify(mMockSettingsNavigation, times(numTimes))
                .startSettings(eq(mMockActivity), eq(ThemeSettingsFragment.class), notNull());
    }

    // Message sent tests.

    @Test
    public void testSendMessage_OptOut_Sent_Clicked() {
        doTestSendMessage_OptOut_Sent(/* clicked= */ true);
    }

    @Test
    public void testSendMessage_OptOut_Sent_NotClicked() {
        doTestSendMessage_OptOut_Sent(/* clicked= */ false);
    }

    @Test
    public void testSendMessage_OptIn_Sent_Clicked() {
        doTestSendMessage_OptIn_Sent(/* clicked= */ true);
    }

    @Test
    public void testSendMessage_OptIn_Sent_NotClicked() {
        doTestSendMessage_OptIn_Sent(/* clicked= */ false);
    }

    // Message not sent tests.

    @Test
    public void testSendMessage_OptOut_NotSent_DisabledShouldTrigger() {
        doTestSendMessage_NotSent(
                /* optOut= */ true, /* enabled= */ false, /* shouldTrigger= */ true);
    }

    @Test
    public void testSendMessage_OptOut_NotSent_DisabledShouldNotTrigger() {
        doTestSendMessage_NotSent(
                /* optOut= */ true, /* enabled= */ false, /* shouldTrigger= */ false);
    }

    @Test
    public void testSendMessage_OptOut_NotSent_EnabledShouldNotTrigger() {
        doTestSendMessage_NotSent(
                /* optOut= */ true, /* enabled= */ true, /* shouldTrigger= */ false);
    }

    @Test
    public void testSendMessage_OptIn_NotSent_EnabledShouldTrigger() {
        doTestSendMessage_NotSent(
                /* optOut= */ false, /* enabled= */ true, /* shouldTrigger= */ true);
    }

    @Test
    public void testSendMessage_OptIn_NotSent_EnabledShouldNotTrigger() {
        doTestSendMessage_NotSent(
                /* optOut= */ false, /* enabled= */ true, /* shouldTrigger= */ false);
    }

    @Test
    public void testSendMessage_OptIn_NotSent_DisabledShouldNotTrigger() {
        doTestSendMessage_NotSent(
                /* optOut= */ false, /* enabled= */ false, /* shouldTrigger= */ false);
    }

    // Dialog tests.

    @Test
    public void testShowDialog_ShouldNotTrigger() {
        // Feature engagement conditions not met.
        when(mMockTracker.shouldTriggerHelpUI(eq(OPT_OUT_FEATURE))).thenReturn(false);

        // Attempt to send message and fail because feature engagement conditions not met.
        WebContentsDarkModeMessageController.attemptToShowDialog(
                mMockActivity, mMockProfile, TEST_URL, mModalDialogManager);
        verify(mMockTracker, times(1)).notifyEvent(eq(DISABLED_EVENT));
        Assert.assertNull(
                "Shown dialog model should be null, since we should not trigger the dialog.",
                mModalDialogManager.mShownDialogModel);
    }

    @Test
    public void testShowDialog_ShouldTrigger() {
        // Attempt to send message and succeed because feature engagement conditions met.
        WebContentsDarkModeMessageController.attemptToShowDialog(
                mMockActivity, mMockProfile, TEST_URL, mModalDialogManager);
        verify(mMockTracker, times(1)).notifyEvent(eq(DISABLED_EVENT));
        Assert.assertNotNull(
                "Shown dialog model should be non-null, since we trigger the dialog.",
                mModalDialogManager.mShownDialogModel);
    }

    @Test
    public void testDialogController_ClickPositiveButton_FeedbackEnabled() {
        // Enable feedback.
        FeatureList.TestValues testValues = new TestValues();
        testValues.addFieldTrialParamOverride(
                ChromeFeatureList.DARKEN_WEBSITES_CHECKBOX_IN_THEMES_SETTING,
                WebContentsDarkModeMessageController.FEEDBACK_DIALOG_PARAM,
                Boolean.toString(true));
        FeatureList.setTestValues(testValues);

        // Click on positive button.
        WebContentsDarkModeMessageController.attemptToShowDialog(
                mMockActivity, mMockProfile, TEST_URL, mModalDialogManager);
        mModalDialogManager.clickButton(ButtonType.POSITIVE);
        verify(mMockFeedbackLauncher, times(1))
                .showFeedback(eq(mMockActivity), eq(TEST_URL), any(), anyInt(), any());

        // Verify dismissal.
        Assert.assertNull(
                "Shown dialog model should be null after clicking the positive button.",
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
        WebContentsDarkModeMessageController.attemptToShowDialog(
                mMockActivity, mMockProfile, TEST_URL, mModalDialogManager);
        mModalDialogManager.clickButton(ButtonType.POSITIVE);
        verifyLaunchSettings(1);

        // Verify dismissal.
        Assert.assertNull(
                "Shown dialog model should be null after clicking the positive button.",
                mModalDialogManager.mShownDialogModel);
        verify(mMockTracker, times(1)).dismissed(eq(OPT_OUT_FEATURE));
    }

    @Test
    public void testDialogController_ClickNegativeButton() {
        // Click on negative button.
        WebContentsDarkModeMessageController.attemptToShowDialog(
                mMockActivity, mMockProfile, TEST_URL, mModalDialogManager);
        mModalDialogManager.clickButton(ButtonType.NEGATIVE);

        // Verify dismissal.
        Assert.assertNull(
                "Shown dialog model should be null after clicking the negative button.",
                mModalDialogManager.mShownDialogModel);
        verify(mMockTracker, times(1)).dismissed(eq(OPT_OUT_FEATURE));
    }

    @Test
    public void testShowDialog_ClickTitleIcon() {
        // Click on title icon.
        WebContentsDarkModeMessageController.attemptToShowDialog(
                mMockActivity, mMockProfile, TEST_URL, mModalDialogManager);
        mModalDialogManager.clickButton(ButtonType.TITLE_ICON);

        // Verify not dismissed.
        Assert.assertNotNull(
                "Shown dialog model should be non-null after clicking the title icon.",
                mModalDialogManager.mShownDialogModel);
        verify(mMockTracker, never()).dismissed(eq(OPT_OUT_FEATURE));
    }

    @Test
    public void testClickableSpan_SettingsLink() {
        AutoDarkClickableSpan clickableSpan = new AutoDarkClickableSpan(mMockActivity);
        clickableSpan.onClick(null);
        verifyLaunchSettings(1);
    }
}
