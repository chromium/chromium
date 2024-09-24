// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.hats;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.task.test.ShadowPostTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.InMemorySharedPreferences;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;

import java.util.HashMap;
import java.util.Map;

@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = ShadowPostTask.class)
public class SurveyClientUnitTest {
    private static final String TEST_SURVEY_TRIGGER = "test_survey_trigger";
    private static final String TEST_TRIGGER_ID = "triggerId1234";
    private ObservableSupplierImpl<Boolean> mCrashUploadPermissionSupplier;
    private TestSurveyUtils.TestSurveyUiDelegate mSurveyUiDelegate;
    private TestSurveyUtils.TestSurveyController mSurveyController;

    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private ActivityLifecycleDispatcher mLifecycleDispatcher;
    @Mock private UserPrefs.Natives mUserPrefsJniMock;
    @Mock private PrefService mPrefServiceMock;
    @Mock private Activity mActivity;
    @Mock private Profile mProfile;
    @Mock private PrivacyPreferencesManager mPrivacyPreferencesManager;
    @Captor private ArgumentCaptor<PauseResumeWithNativeObserver> mLifecycleObserverCaptor;

    @Before
    public void setup() {
        ProfileManager.setLastUsedProfileForTesting(mProfile);
        mJniMocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsJniMock);
        when(mUserPrefsJniMock.get(mProfile)).thenReturn(mPrefServiceMock);
        when(mPrefServiceMock.getBoolean(Pref.FEEDBACK_SURVEYS_ENABLED)).thenReturn(true);

        mCrashUploadPermissionSupplier = new ObservableSupplierImpl<>(true);
        doReturn(mCrashUploadPermissionSupplier)
                .when(mPrivacyPreferencesManager)
                .getUsageAndCrashReportingPermittedObservableSupplier();

        mSurveyUiDelegate = new TestSurveyUtils.TestSurveyUiDelegate();
        mSurveyController = new TestSurveyUtils.TestSurveyController();
        SurveyClientFactory.initialize(mPrivacyPreferencesManager);
        SurveyMetadata.initializeForTesting(new InMemorySharedPreferences(), null);

        ShadowPostTask.setTestImpl(
                new ShadowPostTask.TestImpl() {
                    @Override
                    public void postDelayedTask(
                            @TaskTraits int taskTraits, Runnable task, long delay) {
                        task.run();
                    }
                });
        ThreadUtils.hasSubtleSideEffectsSetThreadAssertsDisabledForTesting(true);
    }

    @Test
    public void createThroughFactory() {
        SurveyConfig config = newSurveyConfigWithoutPsd();
        SurveyClient client =
                SurveyClientFactory.getInstance().createClient(config, mSurveyUiDelegate, mProfile);

        if (!(client instanceof SurveyClientImpl)) {
            throw new AssertionError(
                    "SurveyClient is with a different class: " + client.getClass().getName());
        }
        assertNotNull("Controller is null.", ((SurveyClientImpl) client).getControllerForTesting());
    }

    @Test
    public void surveyDownloadedSuccess_PresentSuccess_SurveyAccepted() {
        mCrashUploadPermissionSupplier.set(true);

        SurveyConfig config = newSurveyConfigWithoutPsd();
        SurveyClientImpl client =
                new SurveyClientImpl(
                        config,
                        mSurveyUiDelegate,
                        mSurveyController,
                        mCrashUploadPermissionSupplier,
                        mProfile);
        client.showSurvey(mActivity, mLifecycleDispatcher);
        ShadowLooper.idleMainLooper();

        assertTrue(
                "No survey download is requested.", mSurveyController.hasSurveyDownloadInQueue());
        mSurveyController.simulateDownloadFinished(TEST_TRIGGER_ID, true);

        assertTrue("Survey UI delegate isn't showing.", mSurveyUiDelegate.isShowing());
        mSurveyUiDelegate.acceptSurvey();
        assertTrue("Survey should be shown.", mSurveyController.isSurveyShown(TEST_TRIGGER_ID));
        assertFalse(
                "Client should not be destroyed after survey being accepted.",
                client.isDestroyed());
    }

    @Test
    public void doNotDownloadedWhenCrashUploadDisabled() {
        mCrashUploadPermissionSupplier.set(false);

        SurveyConfig config = newSurveyConfigWithoutPsd();
        SurveyClientImpl client =
                new SurveyClientImpl(
                        config,
                        mSurveyUiDelegate,
                        mSurveyController,
                        mCrashUploadPermissionSupplier,
                        mProfile);
        client.showSurvey(mActivity, mLifecycleDispatcher);
        ShadowLooper.idleMainLooper();

        assertFalse(
                "No survey download should be requested.",
                mSurveyController.hasSurveyDownloadInQueue());
    }

    @Test
    public void doNotDownloadedWithThrottling() {
        float probability = 0.0f;
        SurveyConfig config =
                new SurveyConfig(
                        TEST_SURVEY_TRIGGER,
                        TEST_TRIGGER_ID,
                        probability,
                        false,
                        new String[0],
                        new String[0]);
        SurveyClientImpl client =
                new SurveyClientImpl(
                        config,
                        mSurveyUiDelegate,
                        mSurveyController,
                        mCrashUploadPermissionSupplier,
                        mProfile);
        client.showSurvey(mActivity, mLifecycleDispatcher);
        ShadowLooper.idleMainLooper();

        assertFalse(
                "No survey download should be requested.",
                mSurveyController.hasSurveyDownloadInQueue());
    }

    @Test
    public void doNotPresentWhenCrashUploadDisabledAfterDownload() {
        SurveyConfig config = newSurveyConfigWithoutPsd();
        SurveyClientImpl client =
                new SurveyClientImpl(
                        config,
                        mSurveyUiDelegate,
                        mSurveyController,
                        mCrashUploadPermissionSupplier,
                        mProfile);
        client.showSurvey(mActivity, mLifecycleDispatcher);

        mCrashUploadPermissionSupplier.set(false);
        ShadowLooper.idleMainLooper();
        assertFalse(
                "Survey invitation should not shown when crash upload disabled.",
                mSurveyController.isSurveyShown(TEST_TRIGGER_ID));
        verify(
                        mLifecycleDispatcher,
                        never().description(
                                        "Should not observe lifecycle dispatcher when download"
                                                + " result is dropped."))
                .register(any());
    }

    @Test
    public void doNotPresentWhenCrashUploadEnabledButPolicyDisabled() {
        when(mPrefServiceMock.getBoolean(Pref.FEEDBACK_SURVEYS_ENABLED)).thenReturn(false);
        SurveyConfig config = newSurveyConfigWithoutPsd();
        SurveyClientImpl client =
                new SurveyClientImpl(
                        config,
                        mSurveyUiDelegate,
                        mSurveyController,
                        mCrashUploadPermissionSupplier,
                        mProfile);
        client.showSurvey(mActivity, mLifecycleDispatcher);

        mCrashUploadPermissionSupplier.set(true);
        ShadowLooper.idleMainLooper();
        assertFalse(
                "Survey invitation should not shown when crash upload disabled.",
                mSurveyController.isSurveyShown(TEST_TRIGGER_ID));
        verify(
                        mLifecycleDispatcher,
                        never().description(
                                        "Should not observe lifecycle dispatcher when download"
                                                + " result is dropped."))
                .register(any());
    }

    @Test
    public void doNotPresentWhenCrashUploadDisabledButPolicyEnabled() {
        when(mPrefServiceMock.getBoolean(Pref.FEEDBACK_SURVEYS_ENABLED)).thenReturn(true);
        SurveyConfig config = newSurveyConfigWithoutPsd();
        SurveyClientImpl client =
                new SurveyClientImpl(
                        config,
                        mSurveyUiDelegate,
                        mSurveyController,
                        mCrashUploadPermissionSupplier,
                        mProfile);
        client.showSurvey(mActivity, mLifecycleDispatcher);

        mCrashUploadPermissionSupplier.set(false);
        ShadowLooper.idleMainLooper();
        assertFalse(
                "Survey invitation should not shown when crash upload disabled.",
                mSurveyController.isSurveyShown(TEST_TRIGGER_ID));
        verify(
                        mLifecycleDispatcher,
                        never().description(
                                        "Should not observe lifecycle dispatcher when download"
                                                + " result is dropped."))
                .register(any());
    }

    @Test
    public void destroyWhenDownloadFailed() {
        SurveyConfig config = newSurveyConfigWithoutPsd();
        SurveyClientImpl client =
                new SurveyClientImpl(
                        config,
                        mSurveyUiDelegate,
                        mSurveyController,
                        mCrashUploadPermissionSupplier,
                        mProfile);
        client.showSurvey(mActivity, mLifecycleDispatcher);
        ShadowLooper.idleMainLooper();

        mSurveyController.simulateDownloadFinished(TEST_TRIGGER_ID, false);
        assertTrue("Client should be destroyed when download failed.", client.isDestroyed());
    }

    @Test
    public void destroyWhenPresentationFailed() {
        mSurveyUiDelegate.setPresentationWillFail();

        SurveyConfig config = newSurveyConfigWithoutPsd();
        SurveyClientImpl client =
                new SurveyClientImpl(
                        config,
                        mSurveyUiDelegate,
                        mSurveyController,
                        mCrashUploadPermissionSupplier,
                        mProfile);
        client.showSurvey(mActivity, mLifecycleDispatcher);
        ShadowLooper.idleMainLooper();

        mSurveyController.simulateDownloadFinished(TEST_TRIGGER_ID, true);
        assertFalse("Survey UI should not shown.", mSurveyUiDelegate.isShowing());
        assertTrue("Client should be destroyed after presentation failed.", client.isDestroyed());
    }

    @Test
    public void destroyWhenSurveyDeclined() {
        SurveyConfig config = newSurveyConfigWithoutPsd();
        SurveyClientImpl client =
                new SurveyClientImpl(
                        config,
                        mSurveyUiDelegate,
                        mSurveyController,
                        mCrashUploadPermissionSupplier,
                        mProfile);
        client.showSurvey(mActivity, mLifecycleDispatcher);
        ShadowLooper.idleMainLooper();

        mSurveyController.simulateDownloadFinished(TEST_TRIGGER_ID, true);
        mSurveyUiDelegate.dismiss();
        assertTrue("Client should be destroyed after survey is declined.", client.isDestroyed());
    }

    @Test
    public void dismissByLifecycleObserver() {
        SurveyConfig config = newSurveyConfigWithoutPsd();
        SurveyClientImpl client =
                new SurveyClientImpl(
                        config,
                        mSurveyUiDelegate,
                        mSurveyController,
                        mCrashUploadPermissionSupplier,
                        mProfile);
        client.showSurvey(mActivity, mLifecycleDispatcher);
        ShadowLooper.idleMainLooper();
        mSurveyController.simulateDownloadFinished(TEST_TRIGGER_ID, true);
        assertTrue("Survey UI should shown.", mSurveyUiDelegate.isShowing());

        verify(mLifecycleDispatcher).register(mLifecycleObserverCaptor.capture());

        mLifecycleObserverCaptor.getValue().onResumeWithNative();
        assertTrue(
                "Survey invitation should still showing since not expired.",
                mSurveyUiDelegate.isShowing());

        // Assume survey expired on resume.
        mSurveyController.simulateSurveyExpired(TEST_TRIGGER_ID);
        mLifecycleObserverCaptor.getValue().onResumeWithNative();
        assertFalse(
                "Survey invitation should be dismissed on resume.", mSurveyUiDelegate.isShowing());
        assertTrue("Client should be destroyed after invitation dismissed.", client.isDestroyed());
    }

    @Test
    public void dismissByCrashUploadSupplier() {
        SurveyConfig config = newSurveyConfigWithoutPsd();
        SurveyClientImpl client =
                new SurveyClientImpl(
                        config,
                        mSurveyUiDelegate,
                        mSurveyController,
                        mCrashUploadPermissionSupplier,
                        mProfile);
        client.showSurvey(mActivity, mLifecycleDispatcher);
        ShadowLooper.idleMainLooper();
        mSurveyController.simulateDownloadFinished(TEST_TRIGGER_ID, true);
        assertTrue("Survey UI should shown.", mSurveyUiDelegate.isShowing());

        mCrashUploadPermissionSupplier.set(false);
        assertFalse(
                "Survey invitation should be dismissed on pause.", mSurveyUiDelegate.isShowing());
        assertTrue("Client should be destroyed after invitation dismissed.", client.isDestroyed());
    }

    @Test
    public void expectCorrectPsd() {
        mCrashUploadPermissionSupplier.set(true);

        final Map<String, String> stringValues = new HashMap<>();
        final Map<String, Boolean> bitValues = new HashMap<>();
        SurveyConfig config =
                new SurveyConfig(
                        TEST_SURVEY_TRIGGER,
                        TEST_TRIGGER_ID,
                        1.0f,
                        false,
                        new String[] {"bitField"},
                        new String[] {"stringField"});
        SurveyClientImpl client =
                new SurveyClientImpl(
                        config,
                        mSurveyUiDelegate,
                        mSurveyController,
                        mCrashUploadPermissionSupplier,
                        mProfile);
        Assert.assertThrows(
                "Expected PSD(s) are missing.",
                AssertionError.class,
                () -> {
                    client.showSurvey(mActivity, mLifecycleDispatcher);
                });
        Assert.assertThrows(
                "Expected PSD(s) are missing.",
                AssertionError.class,
                () -> {
                    client.showSurvey(mActivity, mLifecycleDispatcher, bitValues, stringValues);
                });

        // Provide bit values without strings values.
        stringValues.clear();
        bitValues.clear();
        bitValues.put("bitField", true);
        Assert.assertThrows(
                "Expected PSD(s) are missing.",
                AssertionError.class,
                () -> {
                    client.showSurvey(mActivity, mLifecycleDispatcher, bitValues, stringValues);
                });

        // Provide string values without bit values.
        stringValues.clear();
        bitValues.clear();
        stringValues.put("stringField", "value");
        Assert.assertThrows(
                "Expected PSD(s) are missing.",
                AssertionError.class,
                () -> {
                    client.showSurvey(mActivity, mLifecycleDispatcher, bitValues, stringValues);
                });

        // Provide extra string values without bit values.
        stringValues.clear();
        bitValues.clear();
        stringValues.put("stringField", "value");
        stringValues.put("stringField2", "value2");
        Assert.assertThrows(
                "Extra string PSDs were provided.",
                AssertionError.class,
                () -> {
                    client.showSurvey(mActivity, mLifecycleDispatcher, bitValues, stringValues);
                });

        // Provide both value.
        stringValues.clear();
        bitValues.clear();
        stringValues.put("stringField", "value");
        bitValues.put("bitField", true);
        // All the PSDs are ready. Should not throw errors anymore.
        client.showSurvey(mActivity, mLifecycleDispatcher, bitValues, stringValues);
    }

    private SurveyConfig newSurveyConfigWithoutPsd() {
        return new SurveyConfig(
                TEST_SURVEY_TRIGGER, TEST_TRIGGER_ID, 1.0f, false, new String[0], new String[0]);
    }
}
