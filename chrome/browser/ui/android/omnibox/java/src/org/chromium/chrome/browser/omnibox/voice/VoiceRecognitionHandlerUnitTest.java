// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.voice;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.contains;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.flags.ChromeFeatureList.VOICE_SEARCH_AUDIO_CAPTURE_POLICY;

import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Bundle;

import androidx.annotation.Nullable;
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
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.FeatureList;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteControllerProvider;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteCoordinator;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler.AssistantActionPerformed;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler.AudioPermissionState;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler.VoiceIntentTarget;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler.VoiceInteractionSource;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler.VoiceResult;
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
import org.chromium.ui.permissions.PermissionCallback;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.lang.ref.WeakReference;
import java.util.List;
import java.util.concurrent.ExecutionException;

/**
 * Tests for {@link VoiceRecognitionHandler}. For more tests specific to the
 * Assistant recognition and actions see
 * {@link AssistantVoiceRecognitionHandlerTest}
 * and {@link AssistantActionsHanlerTest} respectively.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {ShadowLog.class, RecognitionTestHelper.ShadowUserPrefs.class})
public class VoiceRecognitionHandlerUnitTest {
    private static final GURL DEFAULT_URL = JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_1);
    private static final GURL DEFAULT_SEARCH_URL =
            JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_URL);
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private @Mock Intent mIntent;
    private @Mock AssistantVoiceSearchService mAssistantVoiceSearchService;
    private @Mock Tab mTab;
    private @Mock VoiceRecognitionHandler.Observer mObserver;
    private @Mock AutocompleteController mAutocompleteController;
    private @Mock AutocompleteMatch mMatch;
    private @Mock AutocompleteCoordinator mAutocompleteCoordinator;
    private @Mock LocationBarDataProvider mDataProvider;
    private @Mock VoiceRecognitionHandler.Delegate mDelegate;
    private @Mock AndroidPermissionDelegate mPermissionDelegate;
    private @Mock Profile mProfile;
    private @Mock PrefService mPrefs;
    private @Mock TemplateUrlService mTemplateUrlService;
    private @Captor ArgumentCaptor<List<VoiceResult>> mVoiceResults;
    private @Captor ArgumentCaptor<WindowAndroid.IntentCallback> mIntentCallback;

    private VoiceRecognitionHandler mHandler;
    private WindowAndroid mWindowAndroid;
    private ObservableSupplierImpl<Profile> mProfileSupplier;
    private FeatureList.TestValues mFeatures;

    @Before
    public void setUp() throws InterruptedException, ExecutionException {
        VoiceRecognitionUtil.setHasRecognitionIntentHandlerForTesting(true);
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        AutocompleteControllerProvider.setControllerForTesting(mAutocompleteController);
        RecognitionTestHelper.ShadowUserPrefs.setPrefService(mPrefs);
        ProfileManager.onProfileAdded(mProfile);
        Profile.setLastUsedProfileForTesting(mProfile);

        doReturn(DEFAULT_SEARCH_URL).when(mTemplateUrlService).getUrlForVoiceSearchQuery(any());

        doReturn(DEFAULT_SEARCH_URL).when(mMatch).getUrl();
        doReturn(true).when(mMatch).isSearchSuggestion();
        doReturn(true).when(mPermissionDelegate).hasPermission(anyString());
        var activity = Robolectric.buildActivity(Activity.class).setup().get();

        mProfileSupplier = new ObservableSupplierImpl<>();
        mWindowAndroid = spy(new WindowAndroid(activity));
        mHandler = spy(new VoiceRecognitionHandler(
                mDelegate, () -> mAssistantVoiceSearchService, () -> {}, mProfileSupplier));
        mHandler.addObserver(mObserver);

        mWindowAndroid.setAndroidPermissionDelegate(mPermissionDelegate);
        doReturn(new WeakReference(activity)).when(mWindowAndroid).getActivity();
        doReturn(mTab).when(mDataProvider).getTab();
        doReturn(DEFAULT_URL).when(mTab).getUrl();
        doReturn(mDataProvider).when(mDelegate).getLocationBarDataProvider();
        doReturn(mAutocompleteCoordinator).when(mDelegate).getAutocompleteCoordinator();
        doReturn(mWindowAndroid).when(mDelegate).getWindowAndroid();

        doReturn(false).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        doReturn(false).when(mAssistantVoiceSearchService).needsEnabledCheck();

        mFeatures = new FeatureList.TestValues();
        FeatureList.setTestValues(mFeatures);
    }

    @After
    public void tearDown() {
        AutocompleteControllerProvider.setControllerForTesting(null);
        mWindowAndroid.destroy();
        // Make sure destroy() propagates.
        // Any cleanup code scheduled for execution via the means of a Handler or PostTask
        // will be taken care of here.
        ShadowLooper.shadowMainLooper().idle();
        mHandler.removeObserver(mObserver);
        VoiceRecognitionHandler.setIsRecognitionIntentPresentForTesting(null);
        FeatureList.setTestValues(null);
        VoiceRecognitionUtil.setIsVoiceSearchEnabledForTesting(null);
        mProfileSupplier.set(null);
        TemplateUrlServiceFactory.setInstanceForTesting(null);
        ProfileManager.onProfileAdded(null);
        Profile.setLastUsedProfileForTesting(null);
    }

    /**
     * Set up AndroidPermissionDelegate to report supplied results when permissions are requested.
     *
     * @param permissionResult The permission result to report.
     */
    void setReportedPermissionResult(int result) {
        doAnswer(inv -> {
            String[] permissions = inv.getArgument(0);
            PermissionCallback callback = inv.getArgument(1);
            var results = new int[permissions.length];
            for (int i = 0; i < permissions.length; i++) {
                results[i] = result;
            }
            callback.onRequestPermissionsResult(permissions, results);
            return 0;
        })
                .when(mPermissionDelegate)
                .requestPermissions(any(), any());
    }

    /**
     * Simulate voice response.
     *
     * @param resultCode The result code the caller will receive.
     * @param text If present, specifies the content of the voice transcription.
     * @param confidence If text is present, this parameter specifies the confidence of the voice
     *         transcription.
     * @param language If present, specifies the language of the transcription.
     */
    void setVoiceResult(
            int resultCode, @Nullable String text, float confidence, @Nullable String language) {
        var intent = new Intent();
        var bundle = new Bundle();
        if (text != null) {
            bundle = RecognitionTestHelper.createDummyBundle(
                    new String[] {text}, new float[] {confidence}, new String[] {language});
        }
        intent.putExtras(bundle);

        doAnswer(inv -> {
            WindowAndroid.IntentCallback cb = inv.getArgument(1);
            cb.onIntentCompleted(resultCode, intent);
            return 0;
        })
                .when(mWindowAndroid)
                .showCancelableIntent(any(Intent.class), mIntentCallback.capture(), any());
    }

    /**
     * Tests for {@link VoiceRecognitionHandler#isVoiceSearchEnabled}.
     */
    @Test
    @SmallTest
    @DisabledTest(message = "the logic being checked never tests for null tab")
    public void testIsVoiceSearchEnabled_FalseOnNullTab() {
        doReturn(null).when(mDataProvider).getTab();
        Assert.assertFalse(mHandler.isVoiceSearchEnabled());
    }

    @Test
    @SmallTest
    public void testIsVoiceSearchEnabled_FalseOnNullDataProvider() {
        doReturn(null).when(mDelegate).getLocationBarDataProvider();
        Assert.assertFalse(mHandler.isVoiceSearchEnabled());
    }

    @Test
    @SmallTest
    public void testIsVoiceSearchEnabled_FalseWhenIncognito() {
        doReturn(true).when(mDataProvider).isIncognito();
        Assert.assertFalse(mHandler.isVoiceSearchEnabled());
    }

    @Test
    @SmallTest
    public void testIsVoiceSearchEnabled_FalseWhenNoPermissionAndCantRequestPermission() {
        doReturn(false).when(mPermissionDelegate).hasPermission(anyString());
        Assert.assertFalse(mHandler.isVoiceSearchEnabled());
        verify(mPermissionDelegate, times(1)).hasPermission(anyString());
        verify(mPermissionDelegate, times(1)).canRequestPermission(anyString());
    }

    @Test
    @SmallTest
    public void testIsVoiceSearchEnabled_Success() {
        doReturn(true).when(mPermissionDelegate).canRequestPermission(anyString());
        doReturn(true).when(mPermissionDelegate).hasPermission(anyString());
        Assert.assertTrue(mHandler.isVoiceSearchEnabled());
    }

    @Test
    @SmallTest
    @Feature("VoiceSearchAudioCapturePolicy")
    public void testIsVoiceSearchEnabled_AllowedByPolicy() {
        mFeatures.addFeatureFlagOverride(VOICE_SEARCH_AUDIO_CAPTURE_POLICY, true);
        doReturn(true).when(mPrefs).getBoolean(Pref.AUDIO_CAPTURE_ALLOWED);
        doReturn(true).when(mPermissionDelegate).canRequestPermission(anyString());
        doReturn(true).when(mPermissionDelegate).canRequestPermission(anyString());
        Assert.assertTrue(mHandler.isVoiceSearchEnabled());
    }

    @Test
    @SmallTest
    @Feature("VoiceSearchAudioCapturePolicy")
    public void testIsVoiceSearchEnabled_DisabledByPolicy() {
        mFeatures.addFeatureFlagOverride(VOICE_SEARCH_AUDIO_CAPTURE_POLICY, true);
        doReturn(false).when(mPrefs).getBoolean(Pref.AUDIO_CAPTURE_ALLOWED);
        doReturn(true).when(mPermissionDelegate).canRequestPermission(anyString());
        doReturn(true).when(mPermissionDelegate).hasPermission(anyString());
        Assert.assertFalse(mHandler.isVoiceSearchEnabled());
    }

    @Test
    @SmallTest
    @Feature("VoiceSearchAudioCapturePolicy")
    public void testIsVoiceSearchEnabled_AudioCapturePolicyAllowsByDefault() {
        mFeatures.addFeatureFlagOverride(VOICE_SEARCH_AUDIO_CAPTURE_POLICY, true);
        doReturn(true).when(mPrefs).getBoolean(Pref.AUDIO_CAPTURE_ALLOWED);
        doReturn(true).when(mPermissionDelegate).canRequestPermission(anyString());
        doReturn(true).when(mPermissionDelegate).hasPermission(anyString());
        Assert.assertTrue(mHandler.isVoiceSearchEnabled());
    }

    @Test
    @SmallTest
    @Feature("VoiceSearchAudioCapturePolicy")
    public void testIsVoiceSearchEnabled_SkipPolicyCheckWhenDisabled() {
        mFeatures.addFeatureFlagOverride(VOICE_SEARCH_AUDIO_CAPTURE_POLICY, false);
        doReturn(false).when(mPrefs).getBoolean(Pref.AUDIO_CAPTURE_ALLOWED);
        doReturn(true).when(mPermissionDelegate).canRequestPermission(anyString());
        doReturn(true).when(mPermissionDelegate).hasPermission(anyString());
        Assert.assertTrue(mHandler.isVoiceSearchEnabled());
    }

    @Test
    @SmallTest
    @Feature("VoiceSearchAudioCapturePolicy")
    public void testIsVoiceSearchEnabled_UpdateAfterProfileSet() {
        mFeatures.addFeatureFlagOverride(VOICE_SEARCH_AUDIO_CAPTURE_POLICY, true);
        doReturn(true).when(mPrefs).getBoolean(Pref.AUDIO_CAPTURE_ALLOWED);
        doReturn(true).when(mPermissionDelegate).canRequestPermission(anyString());
        doReturn(true).when(mPermissionDelegate).hasPermission(anyString());
        verify(mObserver, never()).onVoiceAvailabilityImpacted();
        Assert.assertTrue(mHandler.isVoiceSearchEnabled());

        mProfileSupplier.set(mProfile);
        doReturn(false).when(mPrefs).getBoolean(Pref.AUDIO_CAPTURE_ALLOWED);
        Assert.assertFalse(mHandler.isVoiceSearchEnabled());
        verify(mObserver).onVoiceAvailabilityImpacted();
    }

    /**
     * Tests for {@link VoiceRecognitionHandler#startVoiceRecognition}.
     */
    @Test
    @SmallTest
    public void testStartVoiceRecognition_OnlyUpdateMicButtonStateIfCantRequestPermission() {
        doReturn(false).when(mPermissionDelegate).hasPermission(anyString());
        verify(mObserver, never()).onVoiceAvailabilityImpacted();
        mHandler.startVoiceRecognition(VoiceInteractionSource.OMNIBOX);

        verify(mHandler, never()).recordVoiceSearchStartEvent(anyInt(), anyInt());
        verify(mObserver).onVoiceAvailabilityImpacted();
        verify(mAssistantVoiceSearchService).reportMicPressUserEligibility();
    }

    @Test
    @SmallTest
    public void
    testStartVoiceRecognition_DontUpdateMicIfPermissionsNotGrantedButCanRequestPermissions() {
        doReturn(false).when(mPermissionDelegate).hasPermission(anyString());
        verify(mObserver, never()).onVoiceAvailabilityImpacted();
        doReturn(true).when(mPermissionDelegate).canRequestPermission(anyString());
        setReportedPermissionResult(PackageManager.PERMISSION_DENIED);
        mHandler.startVoiceRecognition(VoiceInteractionSource.OMNIBOX);
        verify(mHandler, never()).recordVoiceSearchStartEvent(anyInt(), anyInt());
        verify(mObserver, never()).onVoiceAvailabilityImpacted();
        verify(mAssistantVoiceSearchService).reportMicPressUserEligibility();
    }

    @Test
    @SmallTest
    public void
    testStartVoiceRecognition_UpdateMicIfPermissionsNotGrantedAndCantRequestPermissions() {
        doReturn(false).when(mPermissionDelegate).hasPermission(anyString());
        verify(mObserver, never()).onVoiceAvailabilityImpacted();
        doReturn(false).when(mPermissionDelegate).canRequestPermission(anyString());
        setReportedPermissionResult(PackageManager.PERMISSION_DENIED);
        mHandler.startVoiceRecognition(VoiceInteractionSource.OMNIBOX);
        verify(mHandler, never()).recordVoiceSearchStartEvent(anyInt(), anyInt());
        verify(mObserver).onVoiceAvailabilityImpacted();
        verify(mAssistantVoiceSearchService).reportMicPressUserEligibility();
    }

    @Test
    @SmallTest
    public void testStartVoiceRecognition_StartsVoiceSearchWithFailedIntent() {
        verify(mObserver, never()).onVoiceAvailabilityImpacted();
        doReturn(WindowAndroid.START_INTENT_FAILURE)
                .when(mWindowAndroid)
                .showCancelableIntent(any(Intent.class), any(), any());

        mHandler.startVoiceRecognition(VoiceInteractionSource.OMNIBOX);

        verify(mHandler, times(1))
                .recordVoiceSearchStartEvent(
                        eq(VoiceInteractionSource.OMNIBOX), eq(VoiceIntentTarget.SYSTEM));
        verify(mObserver).onVoiceAvailabilityImpacted();

        verify(mHandler, times(1))
                .recordVoiceSearchFailureEvent(
                        eq(VoiceInteractionSource.OMNIBOX), eq(VoiceIntentTarget.SYSTEM));
    }

    @Test
    @SmallTest
    public void testStartVoiceRecognition_StartsVoiceSearchWithSuccessfulIntent() {
        setVoiceResult(Activity.RESULT_OK, /* text=*/null, /* confidence=*/0.f, /* language=*/null);
        mHandler.startVoiceRecognition(VoiceInteractionSource.OMNIBOX);
        verify(mHandler, times(1))
                .recordVoiceSearchStartEvent(
                        eq(VoiceInteractionSource.OMNIBOX), eq(VoiceIntentTarget.SYSTEM));
        verify(mObserver, never()).onVoiceAvailabilityImpacted();
    }

    /**
     * Tests for the {@link VoiceRecognitionHandler.VoiceRecognitionCompleteCallback}.
     *
     * These tests are kicked off by
     * {@link VoiceRecognitionHandler#startVoiceRecognition} to test the flow as it would
     * be in reality.
     */
    @Test
    @SmallTest
    public void testCallback_noVoiceSearchResultWithBadResultCode() {
        setVoiceResult(Activity.RESULT_FIRST_USER, /* text=*/null, /* confidence=*/0.f,
                /* language=*/null);

        mHandler.startVoiceRecognition(VoiceInteractionSource.NTP);
        verify(mHandler, times(1))
                .recordVoiceSearchStartEvent(eq(VoiceInteractionSource.NTP), anyInt());
        verify(mHandler, never()).recordVoiceSearchResult(anyInt(), anyBoolean());
        verify(mHandler, times(1))
                .recordVoiceSearchFailureEvent(eq(VoiceInteractionSource.NTP), anyInt());
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "VoiceInteraction.QueryDuration.Android"));
    }

    @Test
    @SmallTest
    public void testCallback_noVoiceSearchResultCanceled() {
        setVoiceResult(
                Activity.RESULT_CANCELED, /* text=*/null, /* confidence=*/0.f, /* language=*/null);

        mHandler.startVoiceRecognition(VoiceInteractionSource.NTP);
        verify(mHandler, times(1))
                .recordVoiceSearchStartEvent(eq(VoiceInteractionSource.NTP), anyInt());
        verify(mHandler, never()).recordVoiceSearchResult(anyInt(), anyBoolean());
        verify(mHandler, times(1))
                .recordVoiceSearchDismissedEvent(eq(VoiceInteractionSource.NTP), anyInt());
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "VoiceInteraction.QueryDuration.Android"));
    }

    @Test
    @SmallTest
    public void testCallback_noVoiceSearchResultWithNullAutocompleteResult() {
        setVoiceResult(Activity.RESULT_OK, /* text=*/null, /* confidence=*/0.f, /* language=*/null);

        mHandler.startVoiceRecognition(VoiceInteractionSource.SEARCH_WIDGET);
        verify(mHandler, times(1))
                .recordVoiceSearchStartEvent(eq(VoiceInteractionSource.SEARCH_WIDGET), anyInt());
        verify(mHandler, times(1)).recordVoiceSearchResult(anyInt(), eq(false));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "VoiceInteraction.QueryDuration.Android"));
    }

    @Test
    @SmallTest
    public void testCallback_noVoiceSearchResultWithNoMatch() {
        setVoiceResult(Activity.RESULT_OK, /* text=*/"", /* confidence=*/1.f, /* language=*/null);
        mHandler.startVoiceRecognition(VoiceInteractionSource.OMNIBOX);
        verify(mHandler, times(1))
                .recordVoiceSearchStartEvent(eq(VoiceInteractionSource.OMNIBOX), anyInt());
        verify(mHandler, times(1)).recordVoiceSearchResult(anyInt(), eq(false));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "VoiceInteraction.QueryDuration.Android"));
    }

    @Test
    @SmallTest
    public void testCallback_successWithLowConfidence() {
        float confidence =
                VoiceRecognitionHandler.VOICE_SEARCH_CONFIDENCE_NAVIGATE_THRESHOLD - 0.01f;
        setVoiceResult(Activity.RESULT_OK, /* text=*/"testing", /* confidence=*/confidence,
                /* language=*/null);

        mHandler.startVoiceRecognition(VoiceInteractionSource.OMNIBOX);
        verify(mHandler, times(1))
                .recordVoiceSearchStartEvent(eq(VoiceInteractionSource.OMNIBOX), anyInt());
        verify(mHandler, times(1))
                .recordVoiceSearchFinishEvent(
                        eq(VoiceInteractionSource.OMNIBOX), eq(VoiceIntentTarget.SYSTEM));
        verify(mHandler).recordVoiceSearchResult(eq(VoiceIntentTarget.SYSTEM), eq(true));
        verify(mHandler).recordVoiceSearchConfidenceValue(
                eq(VoiceIntentTarget.SYSTEM), eq(confidence));
        verify(mHandler, times(1)).recordVoiceSearchResult(anyInt(), anyBoolean());
        verify(mHandler, times(1)).recordVoiceSearchConfidenceValue(anyInt(), anyFloat());

        verify(mAutocompleteCoordinator).onVoiceResults(mVoiceResults.capture());
        RecognitionTestHelper.assertVoiceResultsAreEqual(
                mVoiceResults.getValue(), new String[] {"testing"}, new float[] {confidence}, null);
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "VoiceInteraction.QueryDuration.Android"));
    }

    @Test
    @SmallTest
    public void testCallback_successWithHighConfidence() {
        // Needs to run on the UI thread because we use the TemplateUrlService on success.
        setVoiceResult(Activity.RESULT_OK, /* text=*/"testing",
                VoiceRecognitionHandler.VOICE_SEARCH_CONFIDENCE_NAVIGATE_THRESHOLD, null);
        mHandler.startVoiceRecognition(VoiceInteractionSource.OMNIBOX);
        verify(mHandler, times(1))
                .recordVoiceSearchStartEvent(eq(VoiceInteractionSource.OMNIBOX), anyInt());
        verify(mHandler, times(1))
                .recordVoiceSearchFinishEvent(
                        eq(VoiceInteractionSource.OMNIBOX), eq(VoiceIntentTarget.SYSTEM));
        verify(mHandler).recordVoiceSearchResult(eq(VoiceIntentTarget.SYSTEM), eq(true));
        verify(mHandler).recordVoiceSearchConfidenceValue(eq(VoiceIntentTarget.SYSTEM),
                eq(VoiceRecognitionHandler.VOICE_SEARCH_CONFIDENCE_NAVIGATE_THRESHOLD));
        verify(mHandler, times(1)).recordVoiceSearchResult(anyInt(), anyBoolean());
        verify(mHandler, times(1)).recordVoiceSearchConfidenceValue(anyInt(), anyFloat());
        verify(mAutocompleteCoordinator).onVoiceResults(mVoiceResults.capture());
        RecognitionTestHelper.assertVoiceResultsAreEqual(mVoiceResults.getValue(),
                new String[] {"testing"},
                new float[] {VoiceRecognitionHandler.VOICE_SEARCH_CONFIDENCE_NAVIGATE_THRESHOLD},
                null);
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "VoiceInteraction.QueryDuration.Android"));
    }

    @Test
    @SmallTest
    public void testCallback_successWithLanguages() {
        // Needs to run on the UI thread because we use the TemplateUrlService on success.
        mProfileSupplier.set(mProfile);
        setVoiceResult(Activity.RESULT_OK, /* text=*/"testing",
                VoiceRecognitionHandler.VOICE_SEARCH_CONFIDENCE_NAVIGATE_THRESHOLD, "en-us");
        mHandler.startVoiceRecognition(VoiceInteractionSource.OMNIBOX);
        verify(mHandler, times(1))
                .recordVoiceSearchStartEvent(eq(VoiceInteractionSource.OMNIBOX), anyInt());
        verify(mHandler, times(1))
                .recordVoiceSearchFinishEvent(
                        eq(VoiceInteractionSource.OMNIBOX), eq(VoiceIntentTarget.SYSTEM));
        verify(mHandler).recordVoiceSearchResult(eq(VoiceIntentTarget.SYSTEM), eq(true));
        verify(mHandler).recordVoiceSearchConfidenceValue(eq(VoiceIntentTarget.SYSTEM),
                eq(VoiceRecognitionHandler.VOICE_SEARCH_CONFIDENCE_NAVIGATE_THRESHOLD));
        verify(mHandler, times(1)).recordVoiceSearchResult(anyInt(), anyBoolean());
        verify(mHandler, times(1)).recordVoiceSearchConfidenceValue(anyInt(), anyFloat());
        verify(mAutocompleteCoordinator).onVoiceResults(mVoiceResults.capture());
        RecognitionTestHelper.assertVoiceResultsAreEqual(mVoiceResults.getValue(),
                new String[] {"testing"},
                new float[] {VoiceRecognitionHandler.VOICE_SEARCH_CONFIDENCE_NAVIGATE_THRESHOLD},
                new String[] {"en-us"});
        verify(mDelegate, times(1)).loadUrlFromVoice(contains("&hl=en-us"));
    }

    @Test
    @SmallTest
    public void testParseResults_EmptyBundle() {
        Assert.assertNull(mHandler.convertBundleToVoiceResults(new Bundle()));
    }

    @Test
    @SmallTest
    public void testParseResults_MismatchedTextAndConfidenceScores() {
        Assert.assertNull(
                mHandler.convertBundleToVoiceResults(RecognitionTestHelper.createDummyBundle(
                        new String[] {"blah"}, new float[] {0f, 1f}, null)));
        Assert.assertNull(
                mHandler.convertBundleToVoiceResults(RecognitionTestHelper.createDummyBundle(
                        new String[] {"blah", "foo"}, new float[] {7f}, null)));
        Assert.assertNull(
                mHandler.convertBundleToVoiceResults(RecognitionTestHelper.createDummyBundle(
                        new String[] {"blah", "foo"}, new float[] {7f, 1f}, new String[] {"foo"})));
    }

    @Test
    @SmallTest
    public void testParseResults_ValidBundle() {
        String[] texts = new String[] {"a", "b", "c"};
        float[] confidences = new float[] {0.8f, 1.0f, 1.0f};

        List<VoiceResult> results = mHandler.convertBundleToVoiceResults(
                RecognitionTestHelper.createDummyBundle(texts, confidences, null));

        RecognitionTestHelper.assertVoiceResultsAreEqual(results, texts, confidences, null);
    }

    @Test
    @SmallTest
    public void testParseResults_VoiceResponseURLConversion() {
        doReturn(false).when(mMatch).isSearchSuggestion();
        // Needed to interact with classifier, which requires a valid profile.
        mProfileSupplier.set(mProfile);

        doReturn(mMatch).when(mAutocompleteController).classify(any(), anyBoolean());

        String[] texts = new String[] {"a", "www. b .co .uk", "engadget .com", "www.google.com"};
        float[] confidences = new float[] {1.0f, 1.0f, 1.0f, 1.0f};
        List<VoiceResult> results = mHandler.convertBundleToVoiceResults(
                RecognitionTestHelper.createDummyBundle(texts, confidences, null));

        RecognitionTestHelper.assertVoiceResultsAreEqual(results,
                new String[] {"a", "www.b.co.uk", "engadget.com", "www.google.com"},
                new float[] {1.0f, 1.0f, 1.0f, 1.0f}, null);
    }

    @Test
    @SmallTest
    public void testRecordSuccessMetrics_calledWithNullStartTime() {
        mHandler.setQueryStartTimeForTesting(null);
        mHandler.recordSuccessMetrics(VoiceInteractionSource.OMNIBOX, VoiceIntentTarget.SYSTEM,
                AssistantActionPerformed.TRANSCRIPTION);
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "VoiceInteraction.QueryDuration.Android"));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "VoiceInteraction.QueryDuration.Android.Transcription"));
    }

    @Test
    @SmallTest
    public void testRecordAudioState_deniedCannotAsk() {
        doReturn(false).when(mPermissionDelegate).hasPermission(anyString());
        doReturn(false).when(mPermissionDelegate).canRequestPermission(anyString());
        mHandler.startVoiceRecognition(VoiceInteractionSource.OMNIBOX);
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "VoiceInteraction.AudioPermissionEvent",
                        AudioPermissionState.DENIED_CANNOT_ASK_AGAIN));
    }

    @Test
    @SmallTest
    public void testRecordAudioState_deniedCanAsk() {
        doReturn(false).when(mPermissionDelegate).hasPermission(anyString());
        doReturn(true).when(mPermissionDelegate).canRequestPermission(anyString());
        setReportedPermissionResult(PackageManager.PERMISSION_DENIED);
        mHandler.startVoiceRecognition(VoiceInteractionSource.OMNIBOX);
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "VoiceInteraction.AudioPermissionEvent",
                        AudioPermissionState.DENIED_CAN_ASK_AGAIN));
    }

    @Test
    @SmallTest
    public void testRecordAudioState_granted() {
        doReturn(true).when(mPermissionDelegate).hasPermission(anyString());
        mHandler.startVoiceRecognition(VoiceInteractionSource.OMNIBOX);
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "VoiceInteraction.AudioPermissionEvent", AudioPermissionState.GRANTED));
    }

    @Test
    @SmallTest
    public void testCallback_CalledTwice() {
        setVoiceResult(Activity.RESULT_OK, /* text=*/"", /* confidence=*/1.f, /* language=*/"");
        mHandler.startVoiceRecognition(VoiceInteractionSource.NTP);
        verify(mHandler, never()).recordVoiceSearchUnexpectedResult(anyInt(), anyInt());

        mIntentCallback.getValue().onIntentCompleted(Activity.RESULT_CANCELED, null);
        verify(mHandler, times(1))
                .recordVoiceSearchUnexpectedResult(eq(VoiceInteractionSource.NTP), anyInt());
    }
}
