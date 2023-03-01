// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.voice;

import android.Manifest;
import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.SystemClock;
import android.speech.RecognizerIntent;
import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.CallbackController;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.build.annotations.MockedInTests;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.MutableFlagWithSafeDefault;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteControllerProvider;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteCoordinator;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.translate.TranslateBridge;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.permissions.PermissionCallback;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/**
 * Class containing functionality related to voice search.
 */
@MockedInTests
public class VoiceRecognitionHandler {
    private static final String TAG = "VoiceRecognition";

    /**
     * The minimum confidence threshold that will result in navigating directly to a voice search
     * response (as opposed to treating it like a typed string in the Omnibox).
     */
    @VisibleForTesting
    public static final float VOICE_SEARCH_CONFIDENCE_NAVIGATE_THRESHOLD = 0.9f;
    /**
     * Extra containing the languages for the returned voice transcriptions (ArrayList<String>).
     *
     * This language is only returned for queries handled by Assistant.
     */
    @VisibleForTesting
    static final String VOICE_QUERY_RESULT_LANGUAGES = "android.speech.extra.LANGUAGE";
    /**
     * Extra containing an identifier for the current Assistant experiment.
     *
     * This is only populated for intents initiated via the toolbar button, and is not populated for
     * internal Chrome URLs.
     */
    @VisibleForTesting
    static final String EXTRA_EXPERIMENT_ID = "com.android.chrome.voice.EXPERIMENT_ID";
    /**
     * The parameter from the ASSISTANT_INTENT_EXPERIMENT_ID feature that configures the experiment
     * ID attached via the EXTRA_EXPERIMENT_ID extra.
     */
    @VisibleForTesting
    static final String ASSISTANT_EXPERIMENT_ID_PARAM_NAME = "experiment_id";
    /**
     * Extra containing the source language code of the current page.
     *
     * This is only populated for pages that are translatable and only for intents initiated via the
     * toolbar button.
     */
    @VisibleForTesting
    static final String EXTRA_TRANSLATE_ORIGINAL_LANGUAGE =
            "com.android.chrome.voice.TRANSLATE_ORIGINAL_LANGUAGE";
    /**
     * Extra containing the current language code of the current page.
     *
     * This is only populated for pages that are translatable and only for intents initiated via the
     * toolbar button.
     */
    @VisibleForTesting
    static final String EXTRA_TRANSLATE_CURRENT_LANGUAGE =
            "com.android.chrome.voice.TRANSLATE_CURRENT_LANGUAGE";
    /**
     * Extra containing the user's default target language code.
     *
     * This is only populated for pages that are translatable and only for intents initiated via the
     * toolbar button.
     */
    @VisibleForTesting
    static final String EXTRA_TRANSLATE_TARGET_LANGUAGE =
            "com.android.chrome.voice.TRANSLATE_TARGET_LANGUAGE";
    /**
     * Extra containing the current timestamp (in epoch time) used for tracking intent latency.
     */
    @VisibleForTesting
    static final String EXTRA_INTENT_SENT_TIMESTAMP =
            "com.android.chrome.voice.INTENT_SENT_TIMESTAMP";

    /**
     * Extra containing an integer that indicates which voice entrypoint the intent
     * was initiated
     * from.
     *
     * See VoiceInteractionEventSource for possible values.
     */
    @VisibleForTesting
    static final String EXTRA_VOICE_ENTRYPOINT = "com.android.chrome.voice.VOICE_ENTRYPOINT";

    private static final MutableFlagWithSafeDefault sToolbarMicIphFlag =
            new MutableFlagWithSafeDefault(ChromeFeatureList.TOOLBAR_MIC_IPH_ANDROID, false);
    private static final MutableFlagWithSafeDefault sAssistantIntentExperimentIdFlag =
            new MutableFlagWithSafeDefault(ChromeFeatureList.ASSISTANT_INTENT_EXPERIMENT_ID, false);
    private static final MutableFlagWithSafeDefault sAssistantNonPersonalizedVoiceSearchFlag =
            new MutableFlagWithSafeDefault(
                    ChromeFeatureList.ASSISTANT_NON_PERSONALIZED_VOICE_SEARCH, true);
    private static final MutableFlagWithSafeDefault sAssistantIntentTranslateInfoFlag =
            new MutableFlagWithSafeDefault(
                    ChromeFeatureList.ASSISTANT_INTENT_TRANSLATE_INFO, false);

    private static Boolean sIsRecognitionIntentPresentForTesting;
    private final Delegate mDelegate;
    private Long mQueryStartTimeMs;
    private WebContentsObserver mVoiceSearchWebContentsObserver;
    private Supplier<AssistantVoiceSearchService> mAssistantVoiceSearchServiceSupplier;
    private TranslateBridgeWrapper mTranslateBridgeWrapper;
    private final ObserverList<Observer> mObservers = new ObserverList<>();
    private CallbackController mCallbackController = new CallbackController();
    private ObservableSupplier<Profile> mProfileSupplier;
    private Boolean mIsVoiceSearchEnabledCached;
    private boolean mRegisteredActivityStateListener;

    /**
     * AudioPermissionState defined in tools/metrics/histograms/enums.xml.
     *
     * Do not reorder or remove items, only add new items before NUM_ENTRIES.
     */
    @IntDef({AudioPermissionState.GRANTED, AudioPermissionState.DENIED_CAN_ASK_AGAIN,
            AudioPermissionState.DENIED_CANNOT_ASK_AGAIN})
    public @interface AudioPermissionState {
        // Permissions have been granted and won't be requested this time.
        int GRANTED = 0;
        int DENIED_CAN_ASK_AGAIN = 1;
        int DENIED_CANNOT_ASK_AGAIN = 2;

        // Be sure to also update enums.xml when updating these values.
        int NUM_ENTRIES = 3;
    }

    /**
     * VoiceInteractionEventSource defined in tools/metrics/histograms/enums.xml.
     *
     * Do not reorder or remove items, only add new items before NUM_ENTRIES.
     */
    @IntDef({VoiceInteractionSource.OMNIBOX, VoiceInteractionSource.NTP,
            VoiceInteractionSource.SEARCH_WIDGET, VoiceInteractionSource.TASKS_SURFACE,
            VoiceInteractionSource.TOOLBAR})
    public @interface VoiceInteractionSource {
        int OMNIBOX = 0;
        int NTP = 1;
        int SEARCH_WIDGET = 2;
        int TASKS_SURFACE = 3;
        int TOOLBAR = 4;

        // Be sure to also update enums.xml when updating these values.
        int NUM_ENTRIES = 5;
    }

    /**
     * VoiceIntentTarget defined in tools/metrics/histograms/enums.xml.
     *
     * Do not reorder or remove items, only add new items before NUM_ENTRIES.
     */
    @IntDef({VoiceIntentTarget.SYSTEM, VoiceIntentTarget.ASSISTANT})
    public @interface VoiceIntentTarget {
        int SYSTEM = 0;
        int ASSISTANT = 1;

        // Be sure to also update enums.xml when updating these values.
        int NUM_ENTRIES = 2;
    }

    /**
     * AssistantActionPerformed defined in tools/metrics/histograms/enums.xml.
     *
     * Do not reorder or remove items, only add new items before NUM_ENTRIES.
     */
    @IntDef({AssistantActionPerformed.UNKNOWN, AssistantActionPerformed.TRANSCRIPTION,
            AssistantActionPerformed.TRANSLATE, AssistantActionPerformed.READOUT})
    public @interface AssistantActionPerformed {
        int UNKNOWN = 0;
        int TRANSCRIPTION = 1;
        int TRANSLATE = 2;
        int READOUT = 3;

        // Be sure to also update enums.xml when updating these values.
        int NUM_ENTRIES = 4;
    }

    /**
     * Delegate interface to provide data to this class from the location bar implementation.
     */
    public interface Delegate {
        /**
         * Loads the provided URL, assumes the PageTransition type is TYPED.
         * @param url The URL to load.
         */
        void loadUrlFromVoice(String url);

        /**
         * Sets the query string in the omnibox (ensuring the URL bar has focus and triggering
         * autocomplete for the specified query) as if the user typed it.
         * @param query The query to be set in the omnibox.
         */
        void setSearchQuery(final String query);

        /**
         * Grabs a reference to the location data provider from the location bar.
         * @return The {@link LocationBarDataProvider} currently in use by the
         *         {@link LocationBarLayout}.
         */
        LocationBarDataProvider getLocationBarDataProvider();

        /**
         * Grabs a reference to the autocomplete coordinator from the location bar.
         * @return The {@link AutocompleteCoordinator} currently in use by the
         *         {@link LocationBarLayout}.
         */
        // TODO(tedchoc): Limit the visibility of what is passed in here.  This does not need the
        //                full coordinator.  It simply needs a way to pass voice suggestions to the
        //                AutocompleteController.
        AutocompleteCoordinator getAutocompleteCoordinator();

        /**
         * @return The current {@link WindowAndroid}.
         */
        WindowAndroid getWindowAndroid();

        /** Clears omnibox focus, used to display the dialog when the keyboard is shown. */
        void clearOmniboxFocus();

        /** Notifies the delegate that voice recognition could not complete. */
        void notifyVoiceRecognitionCanceled();
    }

    /** Interface for observers interested in updates to the voice state. */
    public interface Observer {
        /**
         * Triggers when an event occurs that impacts availability of the voice
         * recognition, for example audio permissions or policy values change.
         */
        void onVoiceAvailabilityImpacted();
    }

    /**
     * Wraps our usage of the static methods in the {@link TranslateBridge} into a class that can be
     * mocked for testing.
     */
    public static class TranslateBridgeWrapper {
        /**
         * Returns true iff the current tab can be manually translated.
         * Logging should only be performed when this method is called to show the translate menu
         * item.
         */
        public boolean canManuallyTranslate(Tab tab) {
            return TranslateBridge.canManuallyTranslate(tab, /*menuLogging=*/false);
        }

        /**
         * Returns the source language code of the given tab. Empty string if no language was
         * detected yet.
         */
        public String getSourceLanguage(Tab tab) {
            return TranslateBridge.getSourceLanguage(tab);
        }

        /**
         * Returns the current language code of the given tab. Empty string if no language was
         * detected yet.
         */
        public String getCurrentLanguage(Tab tab) {
            return TranslateBridge.getCurrentLanguage(tab);
        }

        /**
         * Returns the best target language based on what the Translate Service knows about the
         * user.
         */
        public String getTargetLanguage() {
            return TranslateBridge.getTargetLanguage();
        }
    }

    /**
     * A storage class that holds voice recognition string matches and confidence scores.
     */
    public static class VoiceResult {
        private final String mMatch;
        private final float mConfidence;
        @Nullable
        private final String mLanguage;

        public VoiceResult(String match, float confidence) {
            this(match, confidence, null);
        }

        /**
         * Creates an instance of a VoiceResult.
         * @param match The text match from the voice recognition.
         * @param confidence The confidence value of the recognition that should go from 0.0 to 1.0.
         * @param language The language of the returned query.
         */
        public VoiceResult(String match, float confidence, @Nullable String language) {
            mMatch = match;
            mConfidence = confidence;
            mLanguage = language;
        }

        /**
         * @return The text match from the voice recognition.
         */
        public String getMatch() {
            return mMatch;
        }

        /**
         * @return The confidence value of the recognition that should go from 0.0 to 1.0.
         */
        public float getConfidence() {
            return mConfidence;
        }

        /**
         * @return The IETF language tag for this result.
         */
        public @Nullable String getLanguage() {
            return mLanguage;
        }
    }

    public VoiceRecognitionHandler(Delegate delegate,
            Supplier<AssistantVoiceSearchService> assistantVoiceSearchServiceSupplier,
            ObservableSupplier<Profile> profileSupplier) {
        mDelegate = delegate;
        mAssistantVoiceSearchServiceSupplier = assistantVoiceSearchServiceSupplier;
        mTranslateBridgeWrapper = new TranslateBridgeWrapper();
        mProfileSupplier = profileSupplier;
        mProfileSupplier.addObserver(
                mCallbackController.makeCancelable(profile -> notifyVoiceAvailabilityImpacted()));
    }

    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    private void notifyVoiceAvailabilityImpacted() {
        for (Observer o : mObservers) {
            o.onVoiceAvailabilityImpacted();
        }
    }

    /**
     * Instantiated when a voice search is performed to monitor the web contents for a navigation
     * to be started so we can notify the render frame that a user gesture has been performed. This
     * allows autoplay of the voice response for search results.
     */
    private final class VoiceSearchWebContentsObserver extends WebContentsObserver {
        public VoiceSearchWebContentsObserver(WebContents webContents) {
            super(webContents);
        }

        /**
         * Forces the user gesture flag to be set on a render frame if the URL being navigated to
         * is a SRP.
         *
         * @param url The URL for the navigation that started, so we can ensure that what we're
         * navigating to is actually a SRP.
         */
        private void setReceivedUserGesture(GURL url) {
            WebContents webContents = mWebContents.get();
            if (webContents == null) return;

            RenderFrameHost renderFrameHost = webContents.getMainFrame();
            if (renderFrameHost == null) return;

            if (!mProfileSupplier.hasValue()) return;
            if (TemplateUrlServiceFactory.getForProfile(mProfileSupplier.get())
                            .isSearchResultsPageFromDefaultSearchProvider(url)) {
                renderFrameHost.notifyUserActivation();
            }
        }

        @Override
        public void didFinishNavigationInPrimaryMainFrame(NavigationHandle navigation) {
            if (navigation.hasCommitted() && !navigation.isErrorPage()) {
                setReceivedUserGesture(navigation.getUrl());
            }
            destroy();
        }
    }

    /**
     * Callback for when we receive voice search results after initiating voice recognition.
     */
    class VoiceRecognitionCompleteCallback implements WindowAndroid.IntentCallback {
        @VoiceInteractionSource
        private final int mSource;

        @VoiceIntentTarget
        private final int mTarget;

        private boolean mCallbackComplete;

        public VoiceRecognitionCompleteCallback(
                @VoiceInteractionSource int source, @VoiceIntentTarget int target) {
            mSource = source;
            mTarget = target;
        }

        // WindowAndroid.IntentCallback implementation:
        @Override
        public void onIntentCompleted(int resultCode, Intent data) {
            if (mCallbackComplete) {
                recordVoiceSearchUnexpectedResult(mSource, mTarget);
                return;
            }

            mCallbackComplete = true;
            if (resultCode == Activity.RESULT_CANCELED) {
                recordVoiceSearchDismissedEvent(mSource, mTarget);
                mDelegate.notifyVoiceRecognitionCanceled();
                return;
            }
            if (resultCode != Activity.RESULT_OK || data.getExtras() == null) {
                recordVoiceSearchFailureEvent(mSource, mTarget);
                mDelegate.notifyVoiceRecognitionCanceled();
                return;
            }

            // Log successful voice use for IPH purposes.
            // Log successful voice use for IPH purposes.
            if (sToolbarMicIphFlag.isEnabled() && mProfileSupplier.hasValue()) {
                // mic shouldn't be visible
                assert !mProfileSupplier.get().isOffTheRecord();
                Tracker tracker = TrackerFactory.getTrackerForProfile(mProfileSupplier.get());
                tracker.notifyEvent(EventConstants.SUCCESSFUL_VOICE_SEARCH);
            }

            recordSuccessMetrics(mSource, mTarget, AssistantActionPerformed.TRANSCRIPTION);
            handleTranscriptionResult(data);
        }

        /**
         * Processes the transcription results within the given Intent, potentially initiating a
         * search or navigation.
         * @param data The {@link Intent} with returned transcription data.
         */
        private void handleTranscriptionResult(Intent data) {
            AutocompleteCoordinator autocompleteCoordinator =
                    mDelegate.getAutocompleteCoordinator();
            assert autocompleteCoordinator != null;

            List<VoiceResult> voiceResults = convertBundleToVoiceResults(data.getExtras());
            autocompleteCoordinator.onVoiceResults(voiceResults);
            VoiceResult topResult =
                    (voiceResults != null && voiceResults.size() > 0) ? voiceResults.get(0) : null;
            if (topResult == null) {
                recordVoiceSearchResult(mTarget, false);
                return;
            }

            String topResultQuery = topResult.getMatch();
            if (TextUtils.isEmpty(topResultQuery)) {
                recordVoiceSearchResult(mTarget, false);
                return;
            }

            recordVoiceSearchResult(mTarget, true);
            recordVoiceSearchConfidenceValue(mTarget, topResult.getConfidence());

            if (topResult.getConfidence() < VOICE_SEARCH_CONFIDENCE_NAVIGATE_THRESHOLD) {
                mDelegate.setSearchQuery(topResultQuery);
                return;
            }

            // Since voice was used, we need to let the frame know that there was a user gesture.
            LocationBarDataProvider locationBarDataProvider =
                    mDelegate.getLocationBarDataProvider();
            Tab currentTab =
                    locationBarDataProvider != null ? locationBarDataProvider.getTab() : null;
            if (currentTab != null) {
                if (mVoiceSearchWebContentsObserver != null) {
                    mVoiceSearchWebContentsObserver.destroy();
                    mVoiceSearchWebContentsObserver = null;
                }
                if (currentTab.getWebContents() != null) {
                    mVoiceSearchWebContentsObserver =
                            new VoiceSearchWebContentsObserver(currentTab.getWebContents());
                }
            }

            if (!mProfileSupplier.hasValue()) return;

            Profile profile = mProfileSupplier.get();
            AutocompleteMatch match =
                    AutocompleteControllerProvider.from(mDelegate.getWindowAndroid())
                            .get(profile)
                            .classify(topResultQuery, false);

            String url;
            if (match == null || match.isSearchSuggestion()) {
                url = TemplateUrlServiceFactory.getForProfile(profile)
                              .getUrlForVoiceSearchQuery(topResultQuery)
                              .getSpec();
                // If a language was returned to us from voice recognition, then use it. Currently,
                // this is only returned when Google is the search engine. Since Google always has
                // the query as a url parameter so appending this param will always be safe.
                if (topResult.getLanguage() != null) {
                    // TODO(crbug.com/1117271): Cleanup these assertions when Assistant launches.
                    assert url.contains("?") : "URL must contain at least one URL param.";
                    assert !url.contains("#") : "URL must not contain a fragment.";
                    url += "&hl=" + topResult.getLanguage();
                }
            } else {
                url = match.getUrl().getSpec();
            }

            mDelegate.loadUrlFromVoice(url);
        }
    }

    /**
     * Returns a String for use as a histogram suffix for histograms split by VoiceIntentTarget.
     * @param target The target of the voice search intent.
     * @return The histogram suffix for the given target. No '.' separator is included.
     */
    private static String getHistogramSuffixForTarget(@VoiceIntentTarget int target) {
        switch (target) {
            case VoiceIntentTarget.SYSTEM:
                return "System";
            case VoiceIntentTarget.ASSISTANT:
                return "Assistant";
            default:
                assert false : "Unknown VoiceIntentTarget: " + target;
                return null;
        }
    }

    /** Convert the android voice intent bundle to a list of result objects. */
    @VisibleForTesting
    protected List<VoiceResult> convertBundleToVoiceResults(Bundle extras) {
        if (extras == null) return null;

        ArrayList<String> strings = extras.getStringArrayList(RecognizerIntent.EXTRA_RESULTS);
        float[] confidences = extras.getFloatArray(RecognizerIntent.EXTRA_CONFIDENCE_SCORES);
        ArrayList<String> languages = extras.getStringArrayList(VOICE_QUERY_RESULT_LANGUAGES);

        if (strings == null || confidences == null) return null;
        if (strings.size() != confidences.length) return null;
        // Langues is optional, so only check the size when it's non-null.
        if (languages != null && languages.size() != strings.size()) return null;

        List<VoiceResult> results = new ArrayList<>();
        for (int i = 0; i < strings.size(); ++i) {
            // Remove any spaces in the voice search match when determining whether it
            // appears to be a URL. This is to prevent cases like (
            // "tech crunch.com" and "www. engadget .com" from not appearing like URLs)
            // from not navigating to the URL.
            // If the string appears to be a URL, then use it instead of the string returned from
            // the voice engine.
            String culledString = strings.get(i).replaceAll(" ", "");

            AutocompleteMatch match = null;
            if (mProfileSupplier.hasValue()) {
                match = AutocompleteControllerProvider.from(mDelegate.getWindowAndroid())
                                .get(mProfileSupplier.get())
                                .classify(culledString, false);
            }

            String urlOrSearchQuery;
            if (match == null || match.isSearchSuggestion()) {
                urlOrSearchQuery = strings.get(i);
            } else {
                urlOrSearchQuery = culledString;
            }

            String language = languages == null ? null : languages.get(i);
            results.add(new VoiceResult(urlOrSearchQuery, confidences[i], language));
        }
        return results;
    }

    /**
     * Triggers a voice recognition intent to allow the user to specify a search query.
     *
     * @param source The source of the voice recognition initiation, such as NTP or omnibox.
     */
    public void startVoiceRecognition(@VoiceInteractionSource int source) {
        ThreadUtils.assertOnUiThread();
        startTrackingQueryDuration();
        WindowAndroid windowAndroid = mDelegate.getWindowAndroid();
        if (windowAndroid == null) {
            mDelegate.notifyVoiceRecognitionCanceled();
            return;
        }
        Activity activity = windowAndroid.getActivity().get();
        if (activity == null) {
            mDelegate.notifyVoiceRecognitionCanceled();
            return;
        }

        if (!VoiceRecognitionUtil.isVoiceSearchPermittedByPolicy(/* strictPolicyCheck=*/true)) {
            mDelegate.notifyVoiceRecognitionCanceled();
            return;
        }

        if (mAssistantVoiceSearchServiceSupplier.hasValue()) {
            mAssistantVoiceSearchServiceSupplier.get().reportMicPressUserEligibility();
            if (startAGSAForAssistantVoiceSearch(activity, windowAndroid, source)) return;
        }

        if (!startSystemForVoiceSearch(activity, windowAndroid, source)) {
            // TODO(wylieb): Emit histogram here to identify how many users are attempting to use
            // voice search, but fail completely.
            Log.w(TAG, "Couldn't find suitable provider for voice searching");
        }
    }

    /**
     * Requests the audio permission and resolves the voice recognition request if necessary.
     *
     * In a situation when permissions can't be requested anymore, or have been requested
     * and the result was a denial without an option to request them again, voice
     * functionality will become unavailable.
     *
     * @param activity The current {@link Activity} that we're requesting permission for.
     * @param windowAndroid Used to request audio permissions from the Android system.
     * @param source The source of the mic button click, used to record metrics.
     * @return Whether audio permissions are granted.
     */
    private boolean ensureAudioPermissionGranted(
            Activity activity, WindowAndroid windowAndroid, @VoiceInteractionSource int source) {
        if (windowAndroid.hasPermission(Manifest.permission.RECORD_AUDIO)) {
            recordAudioPermissionStateEvent(AudioPermissionState.GRANTED);
            return true;
        }
        // If we don't have permission and also can't ask, then there's no more work left other
        // than telling the delegate to update the mic state.
        if (!windowAndroid.canRequestPermission(Manifest.permission.RECORD_AUDIO)) {
            recordAudioPermissionStateEvent(AudioPermissionState.DENIED_CANNOT_ASK_AGAIN);
            notifyVoiceAvailabilityImpacted();
            return false;
        }

        PermissionCallback callback = (permissions, grantResults) -> {
            if (grantResults.length != 1) {
                recordAudioPermissionStateEvent(AudioPermissionState.DENIED_CAN_ASK_AGAIN);
                mDelegate.notifyVoiceRecognitionCanceled();
                return;
            }

            if (grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                // Don't record granted permission here, it will get logged from
                // within startSystemForVoiceSearch call.
                startSystemForVoiceSearch(activity, windowAndroid, source);
            } else if (!windowAndroid.canRequestPermission(Manifest.permission.RECORD_AUDIO)) {
                recordAudioPermissionStateEvent(AudioPermissionState.DENIED_CANNOT_ASK_AGAIN);
                notifyVoiceAvailabilityImpacted();
                mDelegate.notifyVoiceRecognitionCanceled();
            } else {
                recordAudioPermissionStateEvent(AudioPermissionState.DENIED_CAN_ASK_AGAIN);
                mDelegate.notifyVoiceRecognitionCanceled();
            }
        };
        windowAndroid.requestPermissions(new String[] {Manifest.permission.RECORD_AUDIO}, callback);

        return false;
    }

    /** Start the system-provided service to fulfill the current voice search. */
    private boolean startSystemForVoiceSearch(
            Activity activity, WindowAndroid windowAndroid, @VoiceInteractionSource int source) {
        // Check if we need to request audio permissions. If we don't, then trigger a permissions
        // prompt will appear and startVoiceRecognition will be called again.
        if (!ensureAudioPermissionGranted(activity, windowAndroid, source)) {
            mDelegate.notifyVoiceRecognitionCanceled();
            return false;
        }

        Intent intent = new Intent(RecognizerIntent.ACTION_RECOGNIZE_SPEECH);
        intent.putExtra(
                RecognizerIntent.EXTRA_LANGUAGE_MODEL, RecognizerIntent.LANGUAGE_MODEL_WEB_SEARCH);
        intent.putExtra(RecognizerIntent.EXTRA_CALLING_PACKAGE,
                activity.getComponentName().flattenToString());
        intent.putExtra(RecognizerIntent.EXTRA_WEB_SEARCH_ONLY, true);

        if (!showSpeechRecognitionIntent(windowAndroid, intent, source, VoiceIntentTarget.SYSTEM)) {
            // Requery whether or not the recognition intent can be handled.
            isRecognitionIntentPresent(false);
            notifyVoiceAvailabilityImpacted();
            recordVoiceSearchFailureEvent(source, VoiceIntentTarget.SYSTEM);

            return false;
        }
        return true;
    }

    /**
     * Start AGSA to fulfill the current voice search.
     *
     * @return Whether AGSA was actually started, when false we should fallback to
     *         {@link startSystemForVoiceSearch}.
     */
    private boolean startAGSAForAssistantVoiceSearch(
            Activity activity, WindowAndroid windowAndroid, @VoiceInteractionSource int source) {
        AssistantVoiceSearchService assistantVoiceSearchService =
                mAssistantVoiceSearchServiceSupplier.get();
        if (assistantVoiceSearchService == null) return false;

        // Check if the device meets the requirements to use Assistant voice search.
        if (!assistantVoiceSearchService.canRequestAssistantVoiceSearch()) return false;

        Intent intent = assistantVoiceSearchService.getAssistantVoiceSearchIntent();
        intent.putExtra(EXTRA_VOICE_ENTRYPOINT, source);
        // Allows Assistant to track intent latency.
        intent.putExtra(EXTRA_INTENT_SENT_TIMESTAMP, System.currentTimeMillis());

        if (sAssistantIntentExperimentIdFlag.isEnabled()) {
            attachAssistantExperimentId(intent);
        }

        if (!sAssistantNonPersonalizedVoiceSearchFlag.isEnabled()) {
            if (source == VoiceInteractionSource.TOOLBAR
                    && sAssistantIntentTranslateInfoFlag.isEnabled()) {
                boolean attached = attachTranslateExtras(intent);
                recordTranslateExtrasAttachResult(attached);
            }
        }
        if (!showSpeechRecognitionIntent(
                    windowAndroid, intent, source, VoiceIntentTarget.ASSISTANT)) {
            notifyVoiceAvailabilityImpacted();
            recordVoiceSearchFailureEvent(source, VoiceIntentTarget.ASSISTANT);
            return false;
        }

        return true;
    }

    /**
     * Returns the URL of the tab associated with this VoiceRecognitionHandler or null if it is not
     * available.
     */
    @Nullable
    private String getUrl() {
        LocationBarDataProvider locationBarDataProvider = mDelegate.getLocationBarDataProvider();
        if (locationBarDataProvider == null) return null;

        Tab currentTab = locationBarDataProvider.getTab();
        if (currentTab == null || currentTab.isIncognito()) {
            return null;
        }

        GURL pageUrl = currentTab.getUrl();
        if (!UrlUtilities.isHttpOrHttps(pageUrl)) return null;
        return pageUrl.getSpec();
    }

    /** Adds an Extra used to indicate to Assistant which experiment arm the user is in. */
    private void attachAssistantExperimentId(Intent intent) {
        String experimentId = ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.ASSISTANT_INTENT_EXPERIMENT_ID,
                ASSISTANT_EXPERIMENT_ID_PARAM_NAME);
        if (!TextUtils.isEmpty(experimentId)) {
            intent.putExtra(EXTRA_EXPERIMENT_ID, experimentId);
        }
    }

    /**
     * Includes translate information, if available, as Extras in the given Assistant intent.
     *
     * @return True if the Extras were attached successfully (page was translatable, languages
     *         detected, etc), false otherwise.
     */
    private boolean attachTranslateExtras(Intent intent) {
        LocationBarDataProvider locationBarDataProvider = mDelegate.getLocationBarDataProvider();
        Tab currentTab = locationBarDataProvider != null ? locationBarDataProvider.getTab() : null;
        if (currentTab == null || currentTab.isIncognito()
                || !mTranslateBridgeWrapper.canManuallyTranslate(currentTab)) {
            return false;
        }

        // The page's URL is used to ensure the Translate intent doesn't translate the wrong page.
        String url = getUrl();
        // The presence of source and current language fields are used to indicate whether the
        // page is translated and/or is translatable. Only include them if they are both available.
        String sourceLanguageCode = mTranslateBridgeWrapper.getSourceLanguage(currentTab);
        String currentLanguageCode = mTranslateBridgeWrapper.getCurrentLanguage(currentTab);
        if (TextUtils.isEmpty(url) || TextUtils.isEmpty(sourceLanguageCode)
                || TextUtils.isEmpty(currentLanguageCode)) {
            return false;
        }

        intent.putExtra(EXTRA_TRANSLATE_ORIGINAL_LANGUAGE, sourceLanguageCode);
        intent.putExtra(EXTRA_TRANSLATE_CURRENT_LANGUAGE, currentLanguageCode);

        // The target language is not necessary for Assistant to decide whether to show the
        // translate UI.
        String targetLanguageCode = mTranslateBridgeWrapper.getTargetLanguage();
        if (!TextUtils.isEmpty(targetLanguageCode)) {
            intent.putExtra(EXTRA_TRANSLATE_TARGET_LANGUAGE, targetLanguageCode);
        }
        return true;
    }

    /**
     * Shows a cancelable speech recognition intent, returning a boolean that indicates if it was
     * successfully shown.
     *
     * @param windowAndroid The {@link WindowAndroid} associated with the current {@link Tab}.
     * @param intent The speech recognition {@link Intent}.
     * @param source Where the request to launch this {@link Intent} originated, such as NTP or
     *        omnibox.
     * @param target The intended destination of this {@link Intent}, such as the system voice
     *        transcription service or Assistant.
     * @return True if showing the {@link Intent} was successful.
     */
    private boolean showSpeechRecognitionIntent(WindowAndroid windowAndroid, Intent intent,
            @VoiceInteractionSource int source, @VoiceIntentTarget int target) {
        recordVoiceSearchStartEvent(source, target);
        return windowAndroid.showCancelableIntent(intent,
                       new VoiceRecognitionCompleteCallback(source, target),
                       R.string.voice_search_error)
                >= 0;
    }

    /** Returns whether voice search is enabled on the current tab. */
    public boolean isVoiceSearchEnabled() {
        LocationBarDataProvider locationBarDataProvider = mDelegate.getLocationBarDataProvider();
        if (locationBarDataProvider == null) return false;
        if (locationBarDataProvider.isIncognito()) return false;
        WindowAndroid windowAndroid = mDelegate.getWindowAndroid();
        if (windowAndroid == null) return false;
        if (windowAndroid.getActivity().get() == null) return false;
        if (!VoiceRecognitionUtil.isVoiceSearchPermittedByPolicy(false)) return false;

        if (mIsVoiceSearchEnabledCached == null) {
            mIsVoiceSearchEnabledCached = VoiceRecognitionUtil.isVoiceSearchEnabled(windowAndroid);

            // isVoiceSearchEnabled depends on whether or not the user gives permissions to
            // record audio. This permission can be changed either when we display a UI prompt
            // to request permissions, or when the permissions are changed in Android settings.
            // In both scenarios, the state of the application will change to being paused before
            // the permission is changed, so we invalidate the cache here.
            if (!mRegisteredActivityStateListener) {
                ApplicationStatus.registerApplicationStateListener(newState -> {
                    if (newState == ApplicationState.HAS_PAUSED_ACTIVITIES) {
                        mIsVoiceSearchEnabledCached = null;
                    }
                });
                mRegisteredActivityStateListener = true;
            }
        }

        return mIsVoiceSearchEnabledCached;
    }

    /** Start tracking query duration by capturing when it started */
    private void startTrackingQueryDuration() {
        mQueryStartTimeMs = SystemClock.elapsedRealtime();
    }

    /** Record metrics that are only logged for successful intent responses. */
    @VisibleForTesting
    protected void recordSuccessMetrics(@VoiceInteractionSource int source,
            @VoiceIntentTarget int target, @AssistantActionPerformed int action) {
        // Defensive check to guard against onIntentResult being called more than once. This only
        // happens with assistant experiments. See crbug.com/1116927 for details.
        if (mQueryStartTimeMs == null) return;
        long elapsedTimeMs = SystemClock.elapsedRealtime() - mQueryStartTimeMs;
        mQueryStartTimeMs = null;

        recordVoiceSearchFinishEvent(source, target);
        recordVoiceSearchOpenDuration(target, elapsedTimeMs);
    }

    /**
     * Records the source of a voice search initiation.
     * @param source The source of the voice search, such as NTP or omnibox. Values taken from the
     *        enum VoiceInteractionEventSource in enums.xml.
     * @param target The intended recipient of the intent, such as the system voice transcription
     *        service or Assistant.
     */
    @VisibleForTesting
    protected void recordVoiceSearchStartEvent(
            @VoiceInteractionSource int source, @VoiceIntentTarget int target) {
        RecordHistogram.recordEnumeratedHistogram(
                "VoiceInteraction.StartEventSource", source, VoiceInteractionSource.NUM_ENTRIES);
        RecordHistogram.recordEnumeratedHistogram(
                "VoiceInteraction.StartEventTarget", target, VoiceIntentTarget.NUM_ENTRIES);
    }

    /**
     * Records the source of a successful voice search completion.
     * @param source The source of the voice search, such as NTP or omnibox. Values taken from the
     *        enum VoiceInteractionEventSource in enums.xml.
     * @param target The intended recipient of the intent, such as the system voice transcription
     *        service or Assistant.
     */
    @VisibleForTesting
    protected void recordVoiceSearchFinishEvent(
            @VoiceInteractionSource int source, @VoiceIntentTarget int target) {
        RecordHistogram.recordEnumeratedHistogram(
                "VoiceInteraction.FinishEventSource", source, VoiceInteractionSource.NUM_ENTRIES);
        RecordHistogram.recordEnumeratedHistogram(
                "VoiceInteraction.FinishEventTarget", target, VoiceIntentTarget.NUM_ENTRIES);
    }

    /**
     * Records the source of a dismissed voice search.
     * @param source The source of the voice search, such as NTP or omnibox. Values taken from the
     *        enum VoiceInteractionEventSource in enums.xml.
     * @param target The intended recipient of the intent, such as the system voice transcription
     *        service or Assistant.
     */
    @VisibleForTesting
    protected void recordVoiceSearchDismissedEvent(
            @VoiceInteractionSource int source, @VoiceIntentTarget int target) {
        RecordHistogram.recordEnumeratedHistogram("VoiceInteraction.DismissedEventSource", source,
                VoiceInteractionSource.NUM_ENTRIES);
        RecordHistogram.recordEnumeratedHistogram(
                "VoiceInteraction.DismissedEventTarget", target, VoiceIntentTarget.NUM_ENTRIES);
    }

    /**
     * Records the source of a failed voice search.
     * @param source The source of the voice search, such as NTP or omnibox. Values taken from the
     *        enum VoiceInteractionEventSource in enums.xml.
     * @param target The intended recipient of the intent, such as the system voice transcription
     *        service or Assistant.
     */
    @VisibleForTesting
    protected void recordVoiceSearchFailureEvent(
            @VoiceInteractionSource int source, @VoiceIntentTarget int target) {
        RecordHistogram.recordEnumeratedHistogram(
                "VoiceInteraction.FailureEventSource", source, VoiceInteractionSource.NUM_ENTRIES);
        RecordHistogram.recordEnumeratedHistogram(
                "VoiceInteraction.FailureEventTarget", target, VoiceIntentTarget.NUM_ENTRIES);
    }

    /**
     * Records the source of an unexpected voice search result. Ideally this will always be 0.
     * @param source The source of the voice search, such as NTP or omnibox. Values taken from the
     *        enum VoiceInteractionEventSource in enums.xml.
     * @param target The intended recipient of the intent, such as the system voice transcription
     *        service or Assistant.
     */
    @VisibleForTesting
    protected void recordVoiceSearchUnexpectedResult(
            @VoiceInteractionSource int source, @VoiceIntentTarget int target) {
        RecordHistogram.recordEnumeratedHistogram("VoiceInteraction.UnexpectedResultSource", source,
                VoiceInteractionSource.NUM_ENTRIES);
        RecordHistogram.recordEnumeratedHistogram(
                "VoiceInteraction.UnexpectedResultTarget", target, VoiceIntentTarget.NUM_ENTRIES);
    }

    /**
     * Records the result of a voice search.
     *
     * This also records submetrics split by the intent target.
     *
     * @param target The intended recipient of the intent, such as the system voice transcription
     *        service or Assistant.
     * @param result The result of a voice search, true if results were successfully returned.
     */
    @VisibleForTesting
    protected void recordVoiceSearchResult(@VoiceIntentTarget int target, boolean result) {
        RecordHistogram.recordBooleanHistogram("VoiceInteraction.VoiceSearchResult", result);

        String targetSuffix = getHistogramSuffixForTarget(target);
        if (targetSuffix != null) {
            RecordHistogram.recordBooleanHistogram(
                    "VoiceInteraction.VoiceSearchResult." + targetSuffix, result);
        }
    }

    /**
     * Records the voice search confidence value as a percentage, instead of the 0.0 to 1.0 range
     * we receive.
     *
     * This also records submetrics split by the intent target.
     *
     * @param target The intended recipient of the intent, such as the system voice transcription
     *        service or Assistant.
     * @param value The voice search confidence value we received from 0.0 to 1.0.
     */
    @VisibleForTesting
    protected void recordVoiceSearchConfidenceValue(@VoiceIntentTarget int target, float value) {
        int percentage = Math.round(value * 100f);
        RecordHistogram.recordPercentageHistogram(
                "VoiceInteraction.VoiceResultConfidenceValue", percentage);

        String targetSuffix = getHistogramSuffixForTarget(target);
        if (targetSuffix != null) {
            RecordHistogram.recordPercentageHistogram(
                    "VoiceInteraction.VoiceResultConfidenceValue." + targetSuffix, percentage);
        }
    }

    /**
     * Records the end-to-end voice search duration.
     *
     * This also records submetrics split by the intent target.
     *
     * @param target The intended recipient of the intent, such as the system voice transcription
     *        service or Assistant.
     * @param openDurationMs The duration, in milliseconds, between when a voice intent was
     *        initiated and when its result was returned.
     */
    private void recordVoiceSearchOpenDuration(@VoiceIntentTarget int target, long openDurationMs) {
        RecordHistogram.recordMediumTimesHistogram(
                "VoiceInteraction.QueryDuration.Android", openDurationMs);

        String targetSuffix = getHistogramSuffixForTarget(target);
        if (targetSuffix != null) {
            RecordHistogram.recordMediumTimesHistogram(
                    "VoiceInteraction.QueryDuration.Android.Target." + targetSuffix,
                    openDurationMs);
        }
    }

    /** Records whether translate information was successfully attached to an Assistant intent. */
    @VisibleForTesting
    protected void recordTranslateExtrasAttachResult(boolean result) {
        RecordHistogram.recordBooleanHistogram(
                "VoiceInteraction.AssistantIntent.TranslateExtrasAttached", result);
    }

    /**
     * Records audio permissions state when a system voice recognition is requested.
     * @param permissionsState The current RECORD_AUDIO permission state.
     */
    @VisibleForTesting
    protected void recordAudioPermissionStateEvent(@AudioPermissionState int permissionsState) {
        RecordHistogram.recordEnumeratedHistogram("VoiceInteraction.AudioPermissionEvent",
                permissionsState, AudioPermissionState.NUM_ENTRIES);
    }

    /** Allows for overriding the TranslateBridgeWrapper for test purposes. */
    @VisibleForTesting
    protected void setTranslateBridgeWrapper(TranslateBridgeWrapper wrapper) {
        mTranslateBridgeWrapper = wrapper;
    }

    /**
     * Calls into {@link VoiceRecognitionUtil} to determine whether or not the
     * {@link RecognizerIntent#ACTION_RECOGNIZE_SPEECH} {@link Intent} is handled by any
     * {@link android.app.Activity}s in the system.
     *
     * @param useCachedValue Whether or not to use the cached value from a previous result.
     * @return {@code true} if recognition is supported.  {@code false} otherwise.
     */
    @VisibleForTesting
    protected static boolean isRecognitionIntentPresent(boolean useCachedValue) {
        if (sIsRecognitionIntentPresentForTesting != null) {
            return sIsRecognitionIntentPresentForTesting;
        }
        return VoiceRecognitionUtil.isRecognitionIntentPresent(useCachedValue);
    }

    @VisibleForTesting
    /*package*/ static void setIsRecognitionIntentPresentForTesting(
            Boolean isRecognitionIntentPresent) {
        sIsRecognitionIntentPresentForTesting = isRecognitionIntentPresent;
    }

    /** Sets the start time for testing. */
    void setQueryStartTimeForTesting(Long queryStartTimeMs) {
        mQueryStartTimeMs = queryStartTimeMs;
    }
}
