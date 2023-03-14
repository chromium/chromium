// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.voice;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler.EXTRA_EXPERIMENT_ID;
import static org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler.EXTRA_INTENT_SENT_TIMESTAMP;
import static org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler.EXTRA_VOICE_ENTRYPOINT;

import android.app.Activity;
import android.content.Intent;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLog;

import org.chromium.base.FeatureList;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteControllerProvider;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteCoordinator;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler.VoiceIntentTarget;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler.VoiceInteractionSource;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.permissions.AndroidPermissionDelegate;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.lang.ref.WeakReference;
import java.util.concurrent.ExecutionException;

/**
 * Recognition tests with Assistant for {@link VoiceRecognitionHandler}. See
 * {@link AssistantActionsHandlerTest} and {@link VoiceRecognitionHandlerTests}
 * for the other tests.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {ShadowLog.class, RecognitionTestHelper.ShadowUserPrefs.class})
public class AssistantVoiceRecognitionHandlerUnitTest {
    private static final GURL DEFAULT_URL = JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_1);
    private static final GURL DEFAULT_SEARCH_URL =
            JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_URL);

    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private @Mock Intent mIntent;
    private @Mock AssistantVoiceSearchService mAssistantVoiceSearchService;
    private @Mock Tab mTab;
    private @Mock LocationBarDataProvider mDataProvider;
    private @Mock VoiceRecognitionHandler.Delegate mDelegate;
    private @Mock AutocompleteController mController;
    private @Mock PrefService mPrefs;
    private @Mock TemplateUrlService mTemplateUrlService;
    private @Mock AutocompleteCoordinator mAutocompleteCoordinator;
    private @Mock AutocompleteMatch mMatch;
    private @Mock Profile mProfile;
    private @Mock AndroidPermissionDelegate mPermissionDelegate;

    private VoiceRecognitionHandler mHandler;

    private WindowAndroid mWindowAndroid;
    private ObservableSupplierImpl<Profile> mProfileSupplier;
    private FeatureList.TestValues mFeatures;

    @Before
    public void setUp() throws InterruptedException, ExecutionException {
        VoiceRecognitionUtil.setHasRecognitionIntentHandlerForTesting(true);
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        AutocompleteControllerProvider.setControllerForTesting(mController);
        RecognitionTestHelper.ShadowUserPrefs.setPrefService(mPrefs);

        doReturn(mMatch).when(mController).classify(any(), anyBoolean());
        doReturn(DEFAULT_SEARCH_URL).when(mMatch).getUrl();
        doReturn(true).when(mMatch).isSearchSuggestion();
        doReturn(true).when(mPermissionDelegate).hasPermission(anyString());

        var activity = Robolectric.buildActivity(Activity.class).setup().get();

        mProfileSupplier = new ObservableSupplierImpl<>();
        mWindowAndroid = spy(new WindowAndroid(activity));
        mHandler = spy(new VoiceRecognitionHandler(
                mDelegate, () -> mAssistantVoiceSearchService, mProfileSupplier));

        mWindowAndroid.setAndroidPermissionDelegate(mPermissionDelegate);
        doReturn(new WeakReference(activity)).when(mWindowAndroid).getActivity();
        doReturn(DEFAULT_URL).when(mTab).getUrl();
        doReturn(mDataProvider).when(mDelegate).getLocationBarDataProvider();
        doReturn(mAutocompleteCoordinator).when(mDelegate).getAutocompleteCoordinator();
        doReturn(mWindowAndroid).when(mDelegate).getWindowAndroid();

        doReturn(mIntent).when(mAssistantVoiceSearchService).getAssistantVoiceSearchIntent();

        mFeatures = new FeatureList.TestValues();
        FeatureList.setTestValues(mFeatures);
        mFeatures.addFeatureFlagOverride(ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH, true);
    }

    @After
    public void tearDown() {
        mWindowAndroid.destroy();
        FeatureList.setTestValues(null);
        VoiceRecognitionUtil.setHasRecognitionIntentHandlerForTesting(null);
        TemplateUrlServiceFactory.setInstanceForTesting(null);
        AutocompleteControllerProvider.setControllerForTesting(null);
        RecognitionTestHelper.ShadowUserPrefs.setPrefService(null);
    }

    @Test
    @SmallTest
    @Feature({"OmniboxAssistantVoiceSearch"})
    public void testStartVoiceRecognition_StartsPersonalizedAssistantVoiceSearch() {
        mFeatures.addFeatureFlagOverride(
                ChromeFeatureList.ASSISTANT_NON_PERSONALIZED_VOICE_SEARCH, false);

        doReturn(true).when(mAssistantVoiceSearchService).canRequestAssistantVoiceSearch();

        mHandler.startVoiceRecognition(VoiceInteractionSource.OMNIBOX);

        verify(mWindowAndroid, times(1)).showCancelableIntent(eq(mIntent), any(), any());
        verify(mHandler, times(1))
                .recordVoiceSearchStartEvent(
                        eq(VoiceInteractionSource.OMNIBOX), eq(VoiceIntentTarget.ASSISTANT));
        verify(mAssistantVoiceSearchService).reportMicPressUserEligibility();
        verify(mIntent).putExtra(eq(EXTRA_INTENT_SENT_TIMESTAMP), anyLong());
        verify(mIntent).putExtra(EXTRA_VOICE_ENTRYPOINT, VoiceInteractionSource.OMNIBOX);
    }

    @Test
    @SmallTest
    @Feature({"OmniboxAssistantVoiceSearch"})
    public void testStartVoiceRecognition_StartsNonPersonalizedAssistantVoiceSearch() {
        mFeatures.addFeatureFlagOverride(
                ChromeFeatureList.ASSISTANT_NON_PERSONALIZED_VOICE_SEARCH, true);

        doReturn(true).when(mAssistantVoiceSearchService).canRequestAssistantVoiceSearch();

        mHandler.startVoiceRecognition(VoiceInteractionSource.OMNIBOX);

        verify(mWindowAndroid, times(1)).showCancelableIntent(eq(mIntent), any(), any());
        verify(mHandler, times(1))
                .recordVoiceSearchStartEvent(
                        eq(VoiceInteractionSource.OMNIBOX), eq(VoiceIntentTarget.ASSISTANT));
        verify(mAssistantVoiceSearchService).reportMicPressUserEligibility();
        verify(mIntent).putExtra(eq(EXTRA_INTENT_SENT_TIMESTAMP), anyLong());
        verify(mIntent).putExtra(EXTRA_VOICE_ENTRYPOINT, VoiceInteractionSource.OMNIBOX);
    }

    @Test
    @SmallTest
    @Feature("AssistantIntentExperimentId")
    public void testStartVoiceRecognition_AssistantExperimentIdDisabled() {
        mFeatures.addFeatureFlagOverride(ChromeFeatureList.ASSISTANT_INTENT_EXPERIMENT_ID, false);
        mFeatures.addFieldTrialParamOverride(ChromeFeatureList.ASSISTANT_INTENT_EXPERIMENT_ID,
                VoiceRecognitionHandler.ASSISTANT_EXPERIMENT_ID_PARAM_NAME, "test");

        doReturn(true).when(mAssistantVoiceSearchService).canRequestAssistantVoiceSearch();
        mHandler.startVoiceRecognition(VoiceInteractionSource.TOOLBAR);
        verify(mWindowAndroid, times(1)).showCancelableIntent(eq(mIntent), any(), any());
        verify(mIntent, never()).putExtra(eq(EXTRA_EXPERIMENT_ID), anyString());
    }

    @Test
    @SmallTest
    @Feature("AssistantIntentExperimentId")
    public void testStartVoiceRecognition_IncludeExperimentIdInAssistantIntentFromToolbar() {
        mFeatures.addFeatureFlagOverride(ChromeFeatureList.ASSISTANT_INTENT_EXPERIMENT_ID, true);
        mFeatures.addFieldTrialParamOverride(ChromeFeatureList.ASSISTANT_INTENT_EXPERIMENT_ID,
                VoiceRecognitionHandler.ASSISTANT_EXPERIMENT_ID_PARAM_NAME, "test");

        doReturn(true).when(mAssistantVoiceSearchService).canRequestAssistantVoiceSearch();
        mHandler.startVoiceRecognition(VoiceInteractionSource.TOOLBAR);
        verify(mWindowAndroid, times(1)).showCancelableIntent(eq(mIntent), any(), any());
        verify(mIntent).putExtra(EXTRA_EXPERIMENT_ID, "test");
    }

    @Test
    @SmallTest
    @Feature("AssistantIntentExperimentId")
    public void testStartVoiceRecognition_IncludeExperimentIdInAssistantIntentFromNonToolbar() {
        mFeatures.addFeatureFlagOverride(ChromeFeatureList.ASSISTANT_INTENT_EXPERIMENT_ID, true);
        mFeatures.addFieldTrialParamOverride(ChromeFeatureList.ASSISTANT_INTENT_EXPERIMENT_ID,
                VoiceRecognitionHandler.ASSISTANT_EXPERIMENT_ID_PARAM_NAME, "test");

        doReturn(true).when(mAssistantVoiceSearchService).canRequestAssistantVoiceSearch();
        mHandler.startVoiceRecognition(VoiceInteractionSource.OMNIBOX);
        verify(mWindowAndroid, times(1)).showCancelableIntent(eq(mIntent), any(), any());
        verify(mIntent).putExtra(EXTRA_EXPERIMENT_ID, "test");
    }

    @Test
    @SmallTest
    @Feature("VoiceSearchAudioCapturePolicy")
    public void testStartVoiceRecognition_AudioCaptureAllowedByPolicy() {
        mFeatures.addFeatureFlagOverride(ChromeFeatureList.VOICE_SEARCH_AUDIO_CAPTURE_POLICY, true);

        doReturn(true).when(mPrefs).getBoolean(Pref.AUDIO_CAPTURE_ALLOWED);
        mHandler.startVoiceRecognition(VoiceInteractionSource.TOOLBAR);
        verify(mWindowAndroid, times(1)).showCancelableIntent(any(Intent.class), any(), any());
    }

    @Test
    @SmallTest
    @Feature("VoiceSearchAudioCapturePolicy")
    public void testStartVoiceRecognition_AudioCaptureDisabledByPolicy() {
        mFeatures.addFeatureFlagOverride(ChromeFeatureList.VOICE_SEARCH_AUDIO_CAPTURE_POLICY, true);

        doReturn(false).when(mPrefs).getBoolean(Pref.AUDIO_CAPTURE_ALLOWED);
        mHandler.startVoiceRecognition(VoiceInteractionSource.TOOLBAR);
        verify(mWindowAndroid, never()).showCancelableIntent(any(Intent.class), any(), any());
    }

    @Test
    @SmallTest
    @Feature("VoiceSearchAudioCapturePolicy")
    public void testStartVoiceRecognition_AudioCapturePolicyAllowsByDefault() {
        mFeatures.addFeatureFlagOverride(ChromeFeatureList.VOICE_SEARCH_AUDIO_CAPTURE_POLICY, true);

        // Required by strict checker.
        ProfileManager.onProfileAdded(mProfile);
        Profile.setLastUsedProfileForTesting(mProfile);

        doReturn(true).when(mPrefs).getBoolean(Pref.AUDIO_CAPTURE_ALLOWED);

        mHandler.startVoiceRecognition(VoiceInteractionSource.TOOLBAR);
        verify(mWindowAndroid, times(1)).showCancelableIntent(any(Intent.class), any(), any());
    }

    @Test
    @SmallTest
    @Feature("VoiceSearchAudioCapturePolicy")
    public void testStartVoiceRecognition_SkipPolicyWhenFeatureDisabled() {
        mFeatures.addFeatureFlagOverride(
                ChromeFeatureList.VOICE_SEARCH_AUDIO_CAPTURE_POLICY, false);

        // Required by strict checker.
        ProfileManager.onProfileAdded(mProfile);
        Profile.setLastUsedProfileForTesting(mProfile);

        doReturn(false).when(mPrefs).getBoolean(Pref.AUDIO_CAPTURE_ALLOWED);

        mHandler.startVoiceRecognition(VoiceInteractionSource.TOOLBAR);
        verify(mWindowAndroid, times(1)).showCancelableIntent(any(Intent.class), any(), any());
    }
}
