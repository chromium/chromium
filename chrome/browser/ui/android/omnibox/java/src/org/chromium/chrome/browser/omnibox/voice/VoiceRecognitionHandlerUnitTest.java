// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.voice;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.speech.RecognizerIntent;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLog;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.FuseboxSessionState;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.OmniboxStub;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteControllerJni;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteCoordinator;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionIntentHandler.VoiceInteractionSource;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionIntentHandler.VoiceResult;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.BrowserUiUtils;
import org.chromium.components.omnibox.AutocompleteInput;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.components.prefs.PrefChangeRegistrar;
import org.chromium.components.prefs.PrefChangeRegistrarJni;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.permissions.AndroidPermissionDelegate;
import org.chromium.ui.permissions.PermissionCallback;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ExecutionException;

/** Tests for {@link VoiceRecognitionHandler}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {ShadowLog.class})
public class VoiceRecognitionHandlerUnitTest {
    private static final GURL DEFAULT_URL = JUnitTestGURLs.URL_1;
    private static final GURL DEFAULT_SEARCH_URL = JUnitTestGURLs.SEARCH_URL;
    private static final long FAKE_PREF_REGISTRAR = 1L;

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private Tab mTab;
    @Mock private VoiceRecognitionHandler.Observer mObserver;
    @Mock private AutocompleteController mAutocompleteController;
    @Mock private AutocompleteController.Natives mAutocompleteControllerJniMock;
    @Mock private AutocompleteMatch mMatch;
    @Mock private AutocompleteCoordinator mAutocompleteCoordinator;
    @Mock private LocationBarDataProvider mDataProvider;
    @Mock private OmniboxStub mOmniboxStub;
    @Mock private AndroidPermissionDelegate mPermissionDelegate;
    @Mock private FuseboxSessionState mFuseboxSessionState;
    @Mock private Profile mProfile;
    @Mock private Profile mProfile2;
    @Mock private PrefService mPrefs;
    @Mock private PrefChangeRegistrar.Natives mPrefChangeRegistrarJni;
    @Mock private TemplateUrlService mTemplateUrlService;
    @Mock private Runnable mOnCanceled;
    @Mock private VoicePlateIntentFactory mVoicePlateIntentFactory;
    @Captor private ArgumentCaptor<List<VoiceResult>> mVoiceResults;
    @Captor private ArgumentCaptor<WindowAndroid.IntentCallback> mIntentCallback;
    @Captor private ArgumentCaptor<AutocompleteInput> mInputCaptor;
    @Captor private ArgumentCaptor<Intent> mIntentCaptor;

    private final AutocompleteInput mAutocompleteInput = new AutocompleteInput();
    private final Intent mVoicePlateIntent =
            new Intent().setClassName("com.example.voiceplate", "com.example.voiceplate.Activity");
    private VoiceRecognitionIntentHandler mIntentHandler;
    private VoiceRecognitionHandler mHandler;
    private WindowAndroid mWindowAndroid;
    private final SettableMonotonicObservableSupplier<Profile> mProfileSupplier =
            ObservableSuppliers.createMonotonic();

    @Before
    public void setUp() throws InterruptedException, ExecutionException {
        VoiceRecognitionUtil.setHasRecognitionIntentHandlerForTesting(true);
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        AutocompleteControllerJni.setInstanceForTesting(mAutocompleteControllerJniMock);
        lenient()
                .doReturn(mAutocompleteController)
                .when(mAutocompleteControllerJniMock)
                .getForProfile(any());
        UserPrefs.setPrefServiceForTesting(mPrefs);
        PrefChangeRegistrarJni.setInstanceForTesting(mPrefChangeRegistrarJni);
        lenient().doReturn(FAKE_PREF_REGISTRAR).when(mPrefChangeRegistrarJni).init(any(), any());
        lenient().doReturn(mProfile).when(mProfile).getOriginalProfile();
        lenient().doReturn(mProfile2).when(mProfile2).getOriginalProfile();
        lenient().doReturn(true).when(mPrefs).getBoolean(Pref.AUDIO_CAPTURE_ALLOWED);
        ProfileManager.setLastUsedProfileForTesting(mProfile);

        lenient()
                .doReturn(DEFAULT_SEARCH_URL)
                .when(mTemplateUrlService)
                .getUrlForVoiceSearchQuery(any());

        lenient().doReturn(DEFAULT_SEARCH_URL).when(mMatch).getUrl();
        lenient().doReturn(true).when(mMatch).isSearchSuggestion();
        lenient().doReturn(true).when(mPermissionDelegate).hasPermission(anyString());
        var activity = Robolectric.buildActivity(Activity.class).setup().get();

        mWindowAndroid = spy(new WindowAndroid(activity, /* occlusionTrackingAllowed= */ true));
        mIntentHandler = spy(new VoiceRecognitionIntentHandler(mWindowAndroid));
        mHandler =
                new VoiceRecognitionHandler(
                        mOmniboxStub,
                        mDataProvider,
                        mAutocompleteCoordinator,
                        mWindowAndroid,
                        mProfileSupplier,
                        mIntentHandler,
                        mVoicePlateIntentFactory);
        mHandler.addObserver(mObserver);

        mWindowAndroid.setAndroidPermissionDelegate(mPermissionDelegate);
        lenient().doReturn(new WeakReference<>(activity)).when(mWindowAndroid).getActivity();
        lenient().doReturn(mTab).when(mDataProvider).getTab();
        lenient().doReturn(DEFAULT_URL).when(mTab).getUrl();
    }

    @After
    public void tearDown() {
        mWindowAndroid.destroy();
        // Make sure destroy() propagates.
        // Any cleanup code scheduled for execution via the means of a Handler or PostTask
        // will be taken care of here.
        RobolectricUtil.runAllBackgroundAndUi();
        mHandler.removeObserver(mObserver);
    }

    /**
     * Set up AndroidPermissionDelegate to report supplied results when permissions are requested.
     *
     * @param result The permission result to report.
     */
    void setReportedPermissionResult(int result) {
        doAnswer(
                        inv -> {
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
     *     transcription.
     */
    void setVoiceResult(int resultCode, @Nullable String text, float confidence) {
        var intent = new Intent();
        var bundle = new Bundle();
        if (text != null) {
            bundle =
                    RecognitionTestHelper.createPlaceholderBundle(
                            new String[] {text}, new float[] {confidence});
        }
        intent.putExtras(bundle);

        doAnswer(
                        inv -> {
                            WindowAndroid.IntentCallback cb = inv.getArgument(1);
                            cb.onIntentCompleted(resultCode, intent);
                            return 0;
                        })
                .when(mWindowAndroid)
                .showCancelableIntent(any(Intent.class), mIntentCallback.capture(), any());
    }

    /** Makes the Voice Plate intent factory report the request as eligible for the Voice Plate. */
    private void enableVoicePlate() {
        doReturn(mVoicePlateIntent)
                .when(mVoicePlateIntentFactory)
                .createIntent(any(), any(), anyInt());
    }

    @Test
    public void testIsVoiceSearchEnabled_FalseWhenNoPermissionAndCantRequestPermission() {
        doReturn(false).when(mPermissionDelegate).hasPermission(anyString());
        assertFalse(mHandler.isVoiceSearchEnabled());
        verify(mPermissionDelegate).hasPermission(anyString());
        verify(mPermissionDelegate).canRequestPermission(anyString());
    }

    @Test
    public void testIsVoiceSearchEnabled_Success() {
        assertTrue(mHandler.isVoiceSearchEnabled());
    }

    @Test
    public void testIsVoiceSearchEnabled_AllowedByPolicy() {
        doReturn(true).when(mPrefs).getBoolean(Pref.AUDIO_CAPTURE_ALLOWED);
        assertTrue(mHandler.isVoiceSearchEnabled());
    }

    @Test
    public void testIsVoiceSearchEnabled_DisabledByPolicy() {
        doReturn(false).when(mPrefs).getBoolean(Pref.AUDIO_CAPTURE_ALLOWED);
        assertFalse(mHandler.isVoiceSearchEnabled());
    }

    @Test
    public void testIsVoiceSearchEnabled_DisabledWhenAndroidAutoProjected() {
        BrowserUiUtils.setIsAndroidAutoProjectedForTesting(true);
        assertFalse(mHandler.isVoiceSearchEnabled());
    }

    @Test
    public void testIsVoiceSearchEnabled_UpdateAfterProfileSet() {
        doReturn(true).when(mPrefs).getBoolean(Pref.AUDIO_CAPTURE_ALLOWED);
        verify(mObserver, never()).onVoiceAvailabilityImpacted();
        assertTrue(mHandler.isVoiceSearchEnabled());

        mProfileSupplier.set(mProfile);
        doReturn(false).when(mPrefs).getBoolean(Pref.AUDIO_CAPTURE_ALLOWED);
        assertFalse(mHandler.isVoiceSearchEnabled());
        verify(mPrefChangeRegistrarJni).add(FAKE_PREF_REGISTRAR, Pref.AUDIO_CAPTURE_ALLOWED);
        verify(mObserver).onVoiceAvailabilityImpacted();
    }

    @Test
    public void testOnProfileUpdated_recreatesPrefChangeRegistrarAndCleansUpOnDestroy() {
        mProfileSupplier.set(mProfile);
        verify(mPrefChangeRegistrarJni).add(FAKE_PREF_REGISTRAR, Pref.AUDIO_CAPTURE_ALLOWED);
        verify(mObserver).onVoiceAvailabilityImpacted();
        clearInvocations(mPrefChangeRegistrarJni, mObserver);

        mProfileSupplier.set(mProfile2);
        verify(mPrefChangeRegistrarJni).destroy(FAKE_PREF_REGISTRAR);
        verify(mPrefChangeRegistrarJni).add(FAKE_PREF_REGISTRAR, Pref.AUDIO_CAPTURE_ALLOWED);
        verify(mObserver).onVoiceAvailabilityImpacted();
        clearInvocations(mPrefChangeRegistrarJni, mObserver);

        mHandler.destroy();
        verify(mPrefChangeRegistrarJni).destroy(FAKE_PREF_REGISTRAR);
    }

    @Test
    public void
            testStartVoiceRecognition_policyChangesFromAllowedToDisallowed_refusesTranscription() {
        setVoiceResult(
                /* resultCode= */ Activity.RESULT_OK,
                /* text= */ "testing",
                /* confidence= */ VoiceRecognitionHandler
                        .VOICE_SEARCH_CONFIDENCE_NAVIGATE_THRESHOLD);
        doReturn(true).when(mPrefs).getBoolean(Pref.AUDIO_CAPTURE_ALLOWED);

        mHandler.startVoiceRecognition(VoiceInteractionSource.OMNIBOX, mOnCanceled);
        verify(mOnCanceled, never()).run();
        verify(mAutocompleteCoordinator).onVoiceResults(any());
        verify(mOmniboxStub).loadUrlFromVoice("testing");
        clearInvocations(mAutocompleteCoordinator, mOmniboxStub, mIntentHandler);

        doReturn(false).when(mPrefs).getBoolean(Pref.AUDIO_CAPTURE_ALLOWED);
        mHandler.startVoiceRecognition(VoiceInteractionSource.OMNIBOX, mOnCanceled);
        verify(mOnCanceled).run();
        verify(mIntentHandler, never()).recordVoiceSearchStartEvent(anyInt());
        verifyNoInteractions(mAutocompleteCoordinator, mOmniboxStub);
    }

    @Test
    public void
            testStartVoiceRecognition_policyChangesFromDisallowedToAllowed_fulfillsTranscription() {
        setVoiceResult(
                /* resultCode= */ Activity.RESULT_OK,
                /* text= */ "testing",
                /* confidence= */ VoiceRecognitionHandler
                        .VOICE_SEARCH_CONFIDENCE_NAVIGATE_THRESHOLD);
        doReturn(false).when(mPrefs).getBoolean(Pref.AUDIO_CAPTURE_ALLOWED);

        mHandler.startVoiceRecognition(VoiceInteractionSource.OMNIBOX, mOnCanceled);
        verify(mOnCanceled).run();
        verify(mIntentHandler, never()).recordVoiceSearchStartEvent(anyInt());
        verifyNoInteractions(mAutocompleteCoordinator, mOmniboxStub);
        clearInvocations(mOnCanceled, mIntentHandler);

        doReturn(true).when(mPrefs).getBoolean(Pref.AUDIO_CAPTURE_ALLOWED);
        mHandler.startVoiceRecognition(VoiceInteractionSource.OMNIBOX, mOnCanceled);
        verify(mOnCanceled, never()).run();
        verify(mAutocompleteCoordinator).onVoiceResults(any());
        verify(mOmniboxStub).loadUrlFromVoice("testing");
    }

    /** Tests for {@link VoiceRecognitionHandler#startVoiceRecognition}. */
    @Test
    public void testStartVoiceRecognition_OnlyUpdateMicButtonStateIfCantRequestPermission() {
        doReturn(false).when(mPermissionDelegate).hasPermission(anyString());
        verify(mObserver, never()).onVoiceAvailabilityImpacted();
        mHandler.startVoiceRecognition(VoiceInteractionSource.OMNIBOX, () -> {});

        verify(mIntentHandler, never()).recordVoiceSearchStartEvent(anyInt());
        verify(mObserver).onVoiceAvailabilityImpacted();
    }

    @Test
    public void testIgnoreProfileAfterDestroy() {
        mProfileSupplier.set(mProfile);
        verify(mObserver).onVoiceAvailabilityImpacted();
        clearInvocations(mObserver);

        mHandler.destroy();
        mProfileSupplier.set(mProfile2);
        // Stop propagating changes after destroy.
        verifyNoInteractions(mObserver);
    }

    @Test
    public void
            testStartVoiceRecognition_DontUpdateMicIfPermissionsNotGrantedButCanRequestPermissions() {
        doReturn(false).when(mPermissionDelegate).hasPermission(anyString());
        verify(mObserver, never()).onVoiceAvailabilityImpacted();
        doReturn(true).when(mPermissionDelegate).canRequestPermission(anyString());
        setReportedPermissionResult(/* result= */ PackageManager.PERMISSION_DENIED);
        mHandler.startVoiceRecognition(VoiceInteractionSource.OMNIBOX, () -> {});
        verify(mIntentHandler, never()).recordVoiceSearchStartEvent(anyInt());
        verify(mObserver, never()).onVoiceAvailabilityImpacted();
    }

    @Test
    public void
            testStartVoiceRecognition_UpdateMicIfPermissionsNotGrantedAndCantRequestPermissions() {
        doReturn(false).when(mPermissionDelegate).hasPermission(anyString());
        verify(mObserver, never()).onVoiceAvailabilityImpacted();
        doReturn(false).when(mPermissionDelegate).canRequestPermission(anyString());
        mHandler.startVoiceRecognition(VoiceInteractionSource.OMNIBOX, () -> {});
        verify(mIntentHandler, never()).recordVoiceSearchStartEvent(anyInt());
        verify(mObserver).onVoiceAvailabilityImpacted();
    }

    @Test
    public void testStartVoiceRecognition_StartsVoiceSearchWithFailedIntent() {
        verify(mObserver, never()).onVoiceAvailabilityImpacted();
        doReturn(WindowAndroid.START_INTENT_FAILURE)
                .when(mWindowAndroid)
                .showCancelableIntent(any(Intent.class), any(), any());

        mHandler.startVoiceRecognition(VoiceInteractionSource.OMNIBOX, () -> {});

        verify(mIntentHandler).recordVoiceSearchStartEvent(eq(VoiceInteractionSource.OMNIBOX));
        verify(mObserver).onVoiceAvailabilityImpacted();

        verify(mIntentHandler).recordVoiceSearchFailureEvent(eq(VoiceInteractionSource.OMNIBOX));
    }

    @Test
    public void testStartVoiceRecognition_StartsVoiceSearchWithSuccessfulIntent() {
        setVoiceResult(
                /* resultCode= */ Activity.RESULT_OK, /* text= */ null, /* confidence= */ 0.f);
        mHandler.startVoiceRecognition(VoiceInteractionSource.OMNIBOX, () -> {});
        verify(mIntentHandler).recordVoiceSearchStartEvent(eq(VoiceInteractionSource.OMNIBOX));
        verify(mObserver, never()).onVoiceAvailabilityImpacted();
    }

    @Test
    public void testVoicePlate_passesProfileAndSourceToFactory() {
        mProfileSupplier.set(mProfile);
        setVoiceResult(
                /* resultCode= */ Activity.RESULT_CANCELED,
                /* text= */ null,
                /* confidence= */ 0.f);

        mHandler.startVoiceRecognition(VoiceInteractionSource.NTP, mOnCanceled);

        verify(mVoicePlateIntentFactory)
                .createIntent(any(), eq(mProfile), eq(VoiceInteractionSource.NTP));
    }

    @Test
    public void testVoicePlate_lowConfidenceResult_navigatesDirectly() {
        enableVoicePlate();
        setVoiceResult(
                /* resultCode= */ Activity.RESULT_OK, /* text= */ "testing", /* confidence= */ 0.f);

        mHandler.startVoiceRecognition(VoiceInteractionSource.OMNIBOX, mOnCanceled);

        verify(mWindowAndroid).showCancelableIntent(mIntentCaptor.capture(), any(), any());
        assertEquals(mVoicePlateIntent, mIntentCaptor.getValue());
        verify(mIntentHandler, never()).recordVoiceSearchConfidenceValue(anyFloat());
        verify(mOmniboxStub).loadUrlFromVoice("testing");
        verify(mOmniboxStub, never()).beginInput(any());
        verify(mOnCanceled, never()).run();
    }

    @Test
    public void testVoicePlate_resultWithoutConfidenceScores_navigatesDirectly() {
        enableVoicePlate();
        var bundle = new Bundle();
        bundle.putStringArrayList(
                RecognizerIntent.EXTRA_RESULTS, new ArrayList<>(List.of("testing")));
        var data = new Intent().putExtras(bundle);
        doAnswer(
                        inv -> {
                            WindowAndroid.IntentCallback cb = inv.getArgument(1);
                            cb.onIntentCompleted(Activity.RESULT_OK, data);
                            return 0;
                        })
                .when(mWindowAndroid)
                .showCancelableIntent(any(Intent.class), any(), any());

        mHandler.startVoiceRecognition(VoiceInteractionSource.OMNIBOX, mOnCanceled);

        verify(mOmniboxStub).loadUrlFromVoice("testing");
        verify(mOnCanceled, never()).run();
    }

    @Test
    public void testParseResults_ForcedConfidence_IgnoresScores() {
        var bundle = new Bundle();
        bundle.putStringArrayList(
                RecognizerIntent.EXTRA_RESULTS, new ArrayList<>(List.of("a", "b")));

        List<VoiceResult> results =
                VoiceRecognitionIntentHandler.convertBundleToVoiceResults(
                        bundle, /* forceHighConfidence= */ true);

        RecognitionTestHelper.assertVoiceResultsAreEqual(
                results, new String[] {"a", "b"}, new float[] {1.0f, 1.0f});
    }

    @Test
    public void testVoicePlate_canceled_doesNotNavigate() {
        enableVoicePlate();
        setVoiceResult(
                /* resultCode= */ Activity.RESULT_CANCELED,
                /* text= */ null,
                /* confidence= */ 0.f);

        mHandler.startVoiceRecognition(VoiceInteractionSource.OMNIBOX, mOnCanceled);

        verify(mOnCanceled).run();
        verify(mIntentHandler).recordVoiceSearchDismissedEvent(eq(VoiceInteractionSource.OMNIBOX));
        verifyNoInteractions(mOmniboxStub);
    }

    @Test
    public void testVoicePlate_launchFailure_fallsBackToSystemRecognizer() {
        enableVoicePlate();
        setVoiceResult(
                /* resultCode= */ Activity.RESULT_OK, /* text= */ "testing", /* confidence= */ 0.f);
        doReturn(WindowAndroid.START_INTENT_FAILURE)
                .when(mWindowAndroid)
                .showCancelableIntent(eq(mVoicePlateIntent), any(), any());

        mHandler.startVoiceRecognition(VoiceInteractionSource.OMNIBOX, mOnCanceled);

        verify(mWindowAndroid, times(2))
                .showCancelableIntent(mIntentCaptor.capture(), any(), any());
        assertEquals(mVoicePlateIntent, mIntentCaptor.getAllValues().get(0));
        assertNull(mIntentCaptor.getAllValues().get(1).getComponent());
        verify(mIntentHandler).recordVoiceSearchStartEvent(eq(VoiceInteractionSource.OMNIBOX));
        // The system recognizer result is low confidence, so it is not treated as Voice Plate's.
        verify(mOmniboxStub).beginInput(any());
        verify(mOmniboxStub, never()).loadUrlFromVoice(any());
    }

    @Test
    public void testVoicePlate_disallowedByPolicy_doesNotLaunch() {
        enableVoicePlate();
        doReturn(false).when(mPrefs).getBoolean(Pref.AUDIO_CAPTURE_ALLOWED);

        mHandler.startVoiceRecognition(VoiceInteractionSource.OMNIBOX, mOnCanceled);

        verify(mOnCanceled).run();
        verify(mWindowAndroid, never()).showCancelableIntent(any(Intent.class), any(), any());
    }

    @Test
    public void testVoicePlate_audioPermissionUnavailable_doesNotLaunch() {
        enableVoicePlate();
        doReturn(false).when(mPermissionDelegate).hasPermission(anyString());
        doReturn(false).when(mPermissionDelegate).canRequestPermission(anyString());

        mHandler.startVoiceRecognition(VoiceInteractionSource.OMNIBOX, mOnCanceled);

        verify(mObserver).onVoiceAvailabilityImpacted();
        verify(mWindowAndroid, never()).showCancelableIntent(any(Intent.class), any(), any());
    }

    @Test
    public void testVoicePlate_audioPermissionGranted_launchesVoicePlate() {
        enableVoicePlate();
        setVoiceResult(
                /* resultCode= */ Activity.RESULT_OK, /* text= */ "testing", /* confidence= */ 0.f);
        doReturn(false).when(mPermissionDelegate).hasPermission(anyString());
        doReturn(true).when(mPermissionDelegate).canRequestPermission(anyString());
        setReportedPermissionResult(/* result= */ PackageManager.PERMISSION_GRANTED);

        mHandler.startVoiceRecognition(VoiceInteractionSource.OMNIBOX, mOnCanceled);

        verify(mPermissionDelegate).requestPermissions(any(), any());
        verify(mWindowAndroid).showCancelableIntent(eq(mVoicePlateIntent), any(), any());
        verify(mOmniboxStub).loadUrlFromVoice("testing");
    }

    /**
     * Tests for the {@link VoiceRecognitionHandler.VoiceRecognitionCompleteCallback}.
     *
     * <p>These tests are kicked off by {@link VoiceRecognitionHandler#startVoiceRecognition} to
     * test the flow as it would be in reality.
     */
    @Test
    public void testCallback_noVoiceSearchResultWithBadResultCode() {
        setVoiceResult(
                /* resultCode= */ Activity.RESULT_FIRST_USER,
                /* text= */ null,
                /* confidence= */ 0.f);

        mHandler.startVoiceRecognition(VoiceInteractionSource.NTP, () -> {});
        verify(mIntentHandler).recordVoiceSearchStartEvent(eq(VoiceInteractionSource.NTP));
        verify(mIntentHandler, never()).recordVoiceSearchResult(anyBoolean());
        verify(mIntentHandler).recordVoiceSearchFailureEvent(eq(VoiceInteractionSource.NTP));
    }

    @Test
    public void testCallback_noVoiceSearchResultCanceled() {
        setVoiceResult(
                /* resultCode= */ Activity.RESULT_CANCELED,
                /* text= */ null,
                /* confidence= */ 0.f);

        mHandler.startVoiceRecognition(VoiceInteractionSource.NTP, () -> {});
        verify(mIntentHandler).recordVoiceSearchStartEvent(eq(VoiceInteractionSource.NTP));
        verify(mIntentHandler, never()).recordVoiceSearchResult(anyBoolean());
        verify(mIntentHandler).recordVoiceSearchDismissedEvent(eq(VoiceInteractionSource.NTP));
    }

    @Test
    public void testCallback_noVoiceSearchResultWithNullAutocompleteResult() {
        setVoiceResult(
                /* resultCode= */ Activity.RESULT_OK, /* text= */ null, /* confidence= */ 0.f);

        mHandler.startVoiceRecognition(VoiceInteractionSource.SEARCH_WIDGET, () -> {});
        verify(mIntentHandler)
                .recordVoiceSearchStartEvent(eq(VoiceInteractionSource.SEARCH_WIDGET));
        verify(mIntentHandler).recordVoiceSearchResult(eq(false));
    }

    @Test
    public void testCallback_noVoiceSearchResultWithNoMatch() {
        setVoiceResult(/* resultCode= */ Activity.RESULT_OK, /* text= */ "", /* confidence= */ 1.f);
        mHandler.startVoiceRecognition(VoiceInteractionSource.OMNIBOX, () -> {});
        verify(mIntentHandler).recordVoiceSearchStartEvent(eq(VoiceInteractionSource.OMNIBOX));
        verify(mIntentHandler).recordVoiceSearchResult(eq(false));
    }

    @Test
    public void testCallback_successWithLowConfidence() {
        float confidence =
                VoiceRecognitionHandler.VOICE_SEARCH_CONFIDENCE_NAVIGATE_THRESHOLD - 0.01f;
        setVoiceResult(
                /* resultCode= */ Activity.RESULT_OK,
                /* text= */ "testing",
                /* confidence= */ confidence);

        mHandler.startVoiceRecognition(VoiceInteractionSource.OMNIBOX, () -> {});
        verify(mIntentHandler).recordVoiceSearchStartEvent(eq(VoiceInteractionSource.OMNIBOX));
        verify(mIntentHandler).recordVoiceSearchFinishEvent(eq(VoiceInteractionSource.OMNIBOX));
        verify(mIntentHandler).recordVoiceSearchResult(eq(true));
        verify(mIntentHandler).recordVoiceSearchConfidenceValue(eq(confidence));
        verify(mIntentHandler).recordVoiceSearchResult(anyBoolean());
        verify(mIntentHandler).recordVoiceSearchConfidenceValue(anyFloat());

        verify(mAutocompleteCoordinator).onVoiceResults(mVoiceResults.capture());
        RecognitionTestHelper.assertVoiceResultsAreEqual(
                mVoiceResults.getValue(), new String[] {"testing"}, new float[] {confidence});
    }

    @Test
    public void testCallback_successWithHighConfidence() {
        // Needs to run on the UI thread because we use the TemplateUrlService on success.
        setVoiceResult(
                /* resultCode= */ Activity.RESULT_OK,
                /* text= */ "testing",
                /* confidence= */ VoiceRecognitionHandler
                        .VOICE_SEARCH_CONFIDENCE_NAVIGATE_THRESHOLD);
        mHandler.startVoiceRecognition(VoiceInteractionSource.OMNIBOX, () -> {});
        verify(mIntentHandler).recordVoiceSearchStartEvent(eq(VoiceInteractionSource.OMNIBOX));
        verify(mIntentHandler).recordVoiceSearchFinishEvent(eq(VoiceInteractionSource.OMNIBOX));
        verify(mIntentHandler).recordVoiceSearchResult(eq(true));
        verify(mIntentHandler)
                .recordVoiceSearchConfidenceValue(
                        eq(VoiceRecognitionHandler.VOICE_SEARCH_CONFIDENCE_NAVIGATE_THRESHOLD));
        verify(mIntentHandler).recordVoiceSearchResult(anyBoolean());
        verify(mIntentHandler).recordVoiceSearchConfidenceValue(anyFloat());
        verify(mAutocompleteCoordinator).onVoiceResults(mVoiceResults.capture());
        RecognitionTestHelper.assertVoiceResultsAreEqual(
                mVoiceResults.getValue(),
                new String[] {"testing"},
                new float[] {VoiceRecognitionHandler.VOICE_SEARCH_CONFIDENCE_NAVIGATE_THRESHOLD});
    }

    @Test
    public void testParseResults_EmptyBundle() {
        assertNull(
                VoiceRecognitionIntentHandler.convertBundleToVoiceResults(
                        new Bundle(), /* forceHighConfidence= */ false));
    }

    @Test
    public void testParseResults_MismatchedTextAndConfidenceScores() {
        assertNull(
                VoiceRecognitionIntentHandler.convertBundleToVoiceResults(
                        RecognitionTestHelper.createPlaceholderBundle(
                                new String[] {"blah"}, new float[] {0f, 1f}),
                        /* forceHighConfidence= */ false));
        assertNull(
                VoiceRecognitionIntentHandler.convertBundleToVoiceResults(
                        RecognitionTestHelper.createPlaceholderBundle(
                                new String[] {"blah", "foo"}, new float[] {7f}),
                        /* forceHighConfidence= */ false));
    }

    @Test
    public void testParseResults_ValidBundle() {
        String[] texts = new String[] {"a", "b", "c"};
        float[] confidences = new float[] {0.8f, 1.0f, 1.0f};

        List<VoiceResult> results =
                VoiceRecognitionIntentHandler.convertBundleToVoiceResults(
                        RecognitionTestHelper.createPlaceholderBundle(texts, confidences),
                        /* forceHighConfidence= */ false);
        assertEquals(3, results.size());
        RecognitionTestHelper.assertVoiceResultsAreEqual(results, texts, confidences);
    }

    @Test
    public void testParseResults_VoiceResponseURLConversion() {
        doReturn(false).when(mMatch).isSearchSuggestion();
        // Needed to interact with classifier, which requires a valid profile.
        mProfileSupplier.set(mProfile);

        doReturn(mMatch).when(mAutocompleteController).classify(any());

        String[] texts = new String[] {"a", "www. b .co .uk", "engadget .com", "www.google.com"};
        float[] confidences = new float[] {1.0f, 1.0f, 1.0f, 1.0f};
        List<VoiceResult> rawResults =
                VoiceRecognitionIntentHandler.convertBundleToVoiceResults(
                        RecognitionTestHelper.createPlaceholderBundle(texts, confidences),
                        /* forceHighConfidence= */ false);

        mHandler.handleTranscriptionResult(rawResults);

        verify(mAutocompleteCoordinator).onVoiceResults(mVoiceResults.capture());
        RecognitionTestHelper.assertVoiceResultsAreEqual(
                mVoiceResults.getValue(),
                new String[] {"a", "www.b.co.uk", "engadget.com", "www.google.com"},
                new float[] {1.0f, 1.0f, 1.0f, 1.0f});
    }

    @Test
    public void testHandleTranscriptionResult_aimRequestLowConfidence_noUrlNavigation() {
        float confidence = 0;
        setVoiceResult(
                /* resultCode= */ Activity.RESULT_OK,
                /* text= */ "voice text",
                /* confidence= */ confidence);

        mAutocompleteInput.setRequestType(AutocompleteRequestType.AI_MODE);
        doReturn(mFuseboxSessionState).when(mDataProvider).getFuseboxSessionState();
        doReturn(mAutocompleteInput).when(mFuseboxSessionState).getAutocompleteInput();

        mHandler.startVoiceRecognition(VoiceInteractionSource.OMNIBOX, () -> {});

        verify(mOmniboxStub).beginInput(mInputCaptor.capture());
        assertEquals(AutocompleteRequestType.AI_MODE, mInputCaptor.getValue().getRequestType());
    }
}
