// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.voice;

import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.notNull;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.flags.ChromeFeatureList.ASSISTANT_INTENT_TRANSLATE_INFO;
import static org.chromium.chrome.browser.flags.ChromeFeatureList.ASSISTANT_NON_PERSONALIZED_VOICE_SEARCH;
import static org.chromium.chrome.browser.flags.ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH;
import static org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler.EXTRA_TRANSLATE_CURRENT_LANGUAGE;
import static org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler.EXTRA_TRANSLATE_ORIGINAL_LANGUAGE;
import static org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler.EXTRA_TRANSLATE_TARGET_LANGUAGE;

import android.app.Activity;
import android.content.Intent;

import androidx.test.filters.SmallTest;

import org.junit.After;
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
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLog;

import org.chromium.base.FeatureList;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteControllerProvider;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteCoordinator;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler.AssistantActionPerformed;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler.TranslateBridgeWrapper;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler.VoiceIntentTarget;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler.VoiceInteractionSource;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.permissions.AndroidPermissionDelegate;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.lang.ref.WeakReference;
import java.util.concurrent.ExecutionException;

/**
 * Assistant actions tests for {@link VoiceRecognitionHandler}. See
 * {@link AssistantVoiceRecognitionHandlerTest} and
 * {@link VoiceRecognitionHandlerTests}
 * for the other tests.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {ShadowLog.class, RecognitionTestHelper.ShadowUserPrefs.class})
public class AssistantActionsHandlerUnitTest {
    private static final GURL DEFAULT_URL = JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_1);
    private static final GURL DEFAULT_SEARCH_URL =
            JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_URL);
    private static final String DEFAULT_USER_EMAIL = "test@test.com";

    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private @Mock Intent mIntent;
    private @Mock AssistantVoiceSearchService mAssistantVoiceSearchService;
    private @Mock TranslateBridgeWrapper mTranslateBridgeWrapper;
    private @Mock Tab mTab;
    private @Mock VoiceRecognitionHandler.Delegate mDelegate;
    private @Mock AutocompleteController mController;
    private @Mock AutocompleteMatch mMatch;
    private @Mock LocationBarDataProvider mDataProvider;
    private @Mock AutocompleteCoordinator mAutocompleteCoordinator;
    private @Mock AndroidPermissionDelegate mPermissionDelegate;
    private @Captor ArgumentCaptor<WindowAndroid.IntentCallback> mIntentCallback;

    private VoiceRecognitionHandler mHandler;

    private WindowAndroid mWindowAndroid;
    private ObservableSupplierImpl<Profile> mProfileSupplier;
    private FeatureList.TestValues mFeatures;

    @Before
    public void setUp() throws InterruptedException, ExecutionException {
        AutocompleteControllerProvider.setControllerForTesting(mController);
        doReturn(mMatch).when(mController).classify(any(), anyBoolean());
        doReturn(DEFAULT_SEARCH_URL).when(mMatch).getUrl();
        doReturn(true).when(mMatch).isSearchSuggestion();
        doReturn(true).when(mPermissionDelegate).hasPermission(anyString());

        var activity = Robolectric.buildActivity(Activity.class).setup().get();

        mProfileSupplier = new ObservableSupplierImpl<>();
        mWindowAndroid = spy(new WindowAndroid(activity));
        mHandler = spy(new VoiceRecognitionHandler(
                mDelegate, () -> mAssistantVoiceSearchService, () -> {}, mProfileSupplier));

        mWindowAndroid.setAndroidPermissionDelegate(mPermissionDelegate);
        doReturn(new WeakReference(activity)).when(mWindowAndroid).getActivity();
        doReturn(mTab).when(mDataProvider).getTab();
        doReturn(DEFAULT_URL).when(mTab).getUrl();
        doReturn(mDataProvider).when(mDelegate).getLocationBarDataProvider();
        doReturn(mAutocompleteCoordinator).when(mDelegate).getAutocompleteCoordinator();
        doReturn(mWindowAndroid).when(mDelegate).getWindowAndroid();

        doReturn(false).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        doReturn(false).when(mAssistantVoiceSearchService).needsEnabledCheck();
        doReturn(mIntent).when(mAssistantVoiceSearchService).getAssistantVoiceSearchIntent();
        doReturn(DEFAULT_USER_EMAIL).when(mAssistantVoiceSearchService).getUserEmail();

        doReturn(true).when(mTranslateBridgeWrapper).canManuallyTranslate(notNull());
        doReturn("fr").when(mTranslateBridgeWrapper).getSourceLanguage(notNull());
        doReturn("de").when(mTranslateBridgeWrapper).getCurrentLanguage(notNull());
        doReturn("ja").when(mTranslateBridgeWrapper).getTargetLanguage();
        mHandler.setTranslateBridgeWrapper(mTranslateBridgeWrapper);

        mFeatures = new FeatureList.TestValues();
        FeatureList.setTestValues(mFeatures);
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            assertNotNull(mHandler);
            VoiceRecognitionHandler.setIsRecognitionIntentPresentForTesting(null);
            mWindowAndroid.destroy();
        });
        AutocompleteControllerProvider.setControllerForTesting(null);
        FeatureList.setTestValues(null);
    }

    @Test
    @SmallTest
    @Feature("AssistantIntentTranslateInfo")
    public void testStartVoiceRecognition_ToolbarButtonIncludesTranslateInfo() {
        mFeatures.addFeatureFlagOverride(OMNIBOX_ASSISTANT_VOICE_SEARCH, true);
        mFeatures.addFeatureFlagOverride(ASSISTANT_INTENT_TRANSLATE_INFO, true);
        mFeatures.addFeatureFlagOverride(ASSISTANT_NON_PERSONALIZED_VOICE_SEARCH, false);

        doReturn(true).when(mAssistantVoiceSearchService).canRequestAssistantVoiceSearch();
        doReturn(true).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        mHandler.startVoiceRecognition(VoiceInteractionSource.TOOLBAR);

        verify(mWindowAndroid, times(1)).showCancelableIntent(eq(mIntent), any(), any());
        verify(mIntent).putExtra(EXTRA_TRANSLATE_ORIGINAL_LANGUAGE, "fr");
        verify(mIntent).putExtra(EXTRA_TRANSLATE_CURRENT_LANGUAGE, "de");
        verify(mIntent).putExtra(EXTRA_TRANSLATE_TARGET_LANGUAGE, "ja");
    }

    @Test
    @SmallTest
    @Feature("AssistantIntentTranslateInfo")
    public void testStartVoiceRecognition_TranslateExtrasDisabled() {
        mFeatures.addFeatureFlagOverride(ASSISTANT_INTENT_TRANSLATE_INFO, false);
        mFeatures.addFeatureFlagOverride(OMNIBOX_ASSISTANT_VOICE_SEARCH, true);

        doReturn(true).when(mAssistantVoiceSearchService).canRequestAssistantVoiceSearch();
        doReturn(true).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        mHandler.startVoiceRecognition(VoiceInteractionSource.TOOLBAR);

        verify(mWindowAndroid, times(1)).showCancelableIntent(eq(mIntent), any(), any());
        verify(mIntent, never()).putExtra(eq(EXTRA_TRANSLATE_ORIGINAL_LANGUAGE), anyString());
        verify(mIntent, never()).putExtra(eq(EXTRA_TRANSLATE_CURRENT_LANGUAGE), anyString());
        verify(mIntent, never()).putExtra(eq(EXTRA_TRANSLATE_TARGET_LANGUAGE), anyString());
    }

    @Test
    @SmallTest
    @Feature("AssistantIntentTranslateInfo")
    public void testStartVoiceRecognition_NoTranslateExtrasForNonToolbar() {
        mFeatures.addFeatureFlagOverride(OMNIBOX_ASSISTANT_VOICE_SEARCH, true);
        mFeatures.addFeatureFlagOverride(ASSISTANT_INTENT_TRANSLATE_INFO, true);

        doReturn(true).when(mAssistantVoiceSearchService).canRequestAssistantVoiceSearch();
        doReturn(true).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        mHandler.startVoiceRecognition(VoiceInteractionSource.OMNIBOX);

        verify(mWindowAndroid, times(1)).showCancelableIntent(eq(mIntent), any(), any());
        verify(mIntent, never()).putExtra(eq(EXTRA_TRANSLATE_ORIGINAL_LANGUAGE), anyString());
        verify(mIntent, never()).putExtra(eq(EXTRA_TRANSLATE_CURRENT_LANGUAGE), anyString());
        verify(mIntent, never()).putExtra(eq(EXTRA_TRANSLATE_TARGET_LANGUAGE), anyString());
    }

    @Test
    @SmallTest
    @Feature("AssistantIntentTranslateInfo")
    public void testStartVoiceRecognition_NoTranslateExtrasForNonPersonalizedSearch() {
        mFeatures.addFeatureFlagOverride(OMNIBOX_ASSISTANT_VOICE_SEARCH, true);
        mFeatures.addFeatureFlagOverride(ASSISTANT_NON_PERSONALIZED_VOICE_SEARCH, true);
        mFeatures.addFeatureFlagOverride(ASSISTANT_INTENT_TRANSLATE_INFO, true);

        doReturn(true).when(mAssistantVoiceSearchService).canRequestAssistantVoiceSearch();
        doReturn(true).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        mHandler.startVoiceRecognition(VoiceInteractionSource.TOOLBAR);

        verify(mWindowAndroid, times(1)).showCancelableIntent(eq(mIntent), any(), any());
        verify(mIntent, never()).putExtra(eq(EXTRA_TRANSLATE_ORIGINAL_LANGUAGE), anyString());
        verify(mIntent, never()).putExtra(eq(EXTRA_TRANSLATE_CURRENT_LANGUAGE), anyString());
        verify(mIntent, never()).putExtra(eq(EXTRA_TRANSLATE_TARGET_LANGUAGE), anyString());
    }

    @Test
    @SmallTest
    @Feature("AssistantIntentTranslateInfo")
    public void testStartVoiceRecognition_NoTranslateExtrasForNonTranslatePage() {
        mFeatures.addFeatureFlagOverride(OMNIBOX_ASSISTANT_VOICE_SEARCH, true);
        mFeatures.addFeatureFlagOverride(ASSISTANT_INTENT_TRANSLATE_INFO, true);
        mFeatures.addFeatureFlagOverride(ASSISTANT_NON_PERSONALIZED_VOICE_SEARCH, false);

        doReturn(true).when(mAssistantVoiceSearchService).canRequestAssistantVoiceSearch();
        doReturn(true).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        doReturn(false).when(mTranslateBridgeWrapper).canManuallyTranslate(notNull());
        mHandler.startVoiceRecognition(VoiceInteractionSource.TOOLBAR);

        verify(mWindowAndroid, times(1)).showCancelableIntent(eq(mIntent), any(), any());
        verify(mIntent, never()).putExtra(eq(EXTRA_TRANSLATE_ORIGINAL_LANGUAGE), anyString());
        verify(mIntent, never()).putExtra(eq(EXTRA_TRANSLATE_CURRENT_LANGUAGE), anyString());
        verify(mIntent, never()).putExtra(eq(EXTRA_TRANSLATE_TARGET_LANGUAGE), anyString());
    }

    @Test
    @SmallTest
    @Feature("AssistantIntentTranslateInfo")
    public void testStartVoiceRecognition_NoTranslateExtrasWhenLanguagesUndetected() {
        mFeatures.addFeatureFlagOverride(OMNIBOX_ASSISTANT_VOICE_SEARCH, true);
        mFeatures.addFeatureFlagOverride(ASSISTANT_INTENT_TRANSLATE_INFO, true);
        mFeatures.addFeatureFlagOverride(ASSISTANT_NON_PERSONALIZED_VOICE_SEARCH, false);
        doReturn(true).when(mAssistantVoiceSearchService).canRequestAssistantVoiceSearch();
        doReturn(true).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        doReturn(null).when(mTranslateBridgeWrapper).getSourceLanguage(notNull());
        mHandler.startVoiceRecognition(VoiceInteractionSource.TOOLBAR);

        verify(mWindowAndroid, times(1)).showCancelableIntent(eq(mIntent), any(), any());
        verify(mIntent, never()).putExtra(eq(EXTRA_TRANSLATE_ORIGINAL_LANGUAGE), anyString());
        verify(mIntent, never()).putExtra(eq(EXTRA_TRANSLATE_CURRENT_LANGUAGE), anyString());
        verify(mIntent, never()).putExtra(eq(EXTRA_TRANSLATE_TARGET_LANGUAGE), anyString());
    }

    @Test
    @SmallTest
    @Feature("AssistantIntentTranslateInfo")
    public void testStartVoiceRecognition_TranslateInfoTargetLanguageOptional() {
        mFeatures.addFeatureFlagOverride(OMNIBOX_ASSISTANT_VOICE_SEARCH, true);
        mFeatures.addFeatureFlagOverride(ASSISTANT_INTENT_TRANSLATE_INFO, true);
        mFeatures.addFeatureFlagOverride(ASSISTANT_NON_PERSONALIZED_VOICE_SEARCH, false);

        doReturn(true).when(mAssistantVoiceSearchService).canRequestAssistantVoiceSearch();
        doReturn(true).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        doReturn(null).when(mTranslateBridgeWrapper).getTargetLanguage();
        mHandler.startVoiceRecognition(VoiceInteractionSource.TOOLBAR);

        verify(mWindowAndroid, times(1)).showCancelableIntent(eq(mIntent), any(), any());
        verify(mIntent).putExtra(EXTRA_TRANSLATE_ORIGINAL_LANGUAGE, "fr");
        verify(mIntent).putExtra(EXTRA_TRANSLATE_CURRENT_LANGUAGE, "de");
        verify(mIntent, never()).putExtra(eq(EXTRA_TRANSLATE_TARGET_LANGUAGE), anyString());
    }

    @Test
    @SmallTest
    public void testRecordSuccessMetrics_noActionMetrics() {
        mHandler.setQueryStartTimeForTesting(100L);
        mHandler.recordSuccessMetrics(VoiceInteractionSource.OMNIBOX, VoiceIntentTarget.ASSISTANT,
                AssistantActionPerformed.TRANSCRIPTION);
        verify(mHandler, times(1))
                .recordVoiceSearchFinishEvent(
                        eq(VoiceInteractionSource.OMNIBOX), eq(VoiceIntentTarget.ASSISTANT));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "VoiceInteraction.QueryDuration.Android"));
        // Split action metrics should not be recorded.
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "VoiceInteraction.QueryDuration.Android.Transcription"));
    }
}
