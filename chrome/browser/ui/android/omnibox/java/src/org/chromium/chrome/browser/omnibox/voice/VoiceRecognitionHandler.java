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
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ApplicationStateListener;
import org.chromium.base.CallbackController;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.MockedInTests;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteCoordinator;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
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

/** Class containing functionality related to voice search. */
@MockedInTests
public class VoiceRecognitionHandler {
    private static final String TAG = "VoiceRecognition";

    /**
     * The minimum confidence threshold that will result in navigating directly to a voice search
     * response (as opposed to treating it like a typed string in the Omnibox).
     */
    @VisibleForTesting public static final float VOICE_SEARCH_CONFIDENCE_NAVIGATE_THRESHOLD = 0.9f;

    private final Delegate mDelegate;
    private final ObserverList<Observer> mObservers = new ObserverList<>();
    private final ApplicationStateListener mApplicationStateListener =
            this::onApplicationStateChange;
    private Long mQueryStartTimeMs;
    private WebContentsObserver mVoiceSearchWebContentsObserver;
    private CallbackController mCallbackController = new CallbackController();
    private ObservableSupplier<Profile> mProfileSupplier;
    private Boolean mIsVoiceSearchEnabledCached;
    private boolean mRegisteredActivityStateListener;

    /**
     * VoiceInteractionEventSource defined in tools/metrics/histograms/enums.xml.
     *
     * <p>Do not reorder or remove items, only add new items before NUM_ENTRIES.
     */
    @IntDef({
        VoiceInteractionSource.OMNIBOX,
        VoiceInteractionSource.NTP,
        VoiceInteractionSource.SEARCH_WIDGET,
        VoiceInteractionSource.TASKS_SURFACE,
        VoiceInteractionSource.TOOLBAR
    })
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
     * AssistantActionPerformed defined in tools/metrics/histograms/enums.xml.
     *
     * <p>Do not reorder or remove items, only add new items before NUM_ENTRIES.
     */
    @IntDef({
        AssistantActionPerformed.UNKNOWN,
        AssistantActionPerformed.TRANSCRIPTION,
        AssistantActionPerformed.TRANSLATE,
        AssistantActionPerformed.READOUT
    })
    public @interface AssistantActionPerformed {
        int UNKNOWN = 0;
        int TRANSCRIPTION = 1;
        int TRANSLATE = 2;
        int READOUT = 3;

        // Be sure to also update enums.xml when updating these values.
        int NUM_ENTRIES = 4;
    }

    /** Delegate interface to provide data to this class from the location bar implementation. */
    public interface Delegate {
        /**
         * Loads the provided URL, assumes the PageTransition type is TYPED.
         *
         * @param url The URL to load.
         */
        void loadUrlFromVoice(String url);

        /**
         * Sets the query string in the omnibox (ensuring the URL bar has focus and triggering
         * autocomplete for the specified query) as if the user typed it.
         *
         * @param query The query to be set in the omnibox.
         */
        void setSearchQuery(final String query);

        /**
         * Grabs a reference to the location data provider from the location bar.
         *
         * @return The {@link LocationBarDataProvider} currently in use by the {@link
         *     LocationBarLayout}.
         */
        LocationBarDataProvider getLocationBarDataProvider();

        /**
         * Grabs a reference to the autocomplete coordinator from the location bar.
         *
         * @return The {@link AutocompleteCoordinator} currently in use by the {@link
         *     LocationBarLayout}.
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
         * Triggers when an event occurs that impacts availability of the voice recognition, for
         * example audio permissions or policy values change.
         */
        void onVoiceAvailabilityImpacted();
    }

    /** A storage class that holds voice recognition string matches and confidence scores. */
    public static class VoiceResult {
        private final String mMatch;
        private final float mConfidence;

        public VoiceResult(String match, float confidence) {
            mMatch = match;
            mConfidence = confidence;
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
    }

    public VoiceRecognitionHandler(Delegate delegate, ObservableSupplier<Profile> profileSupplier) {
        mDelegate = delegate;
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

    public void destroy() {
        if (mCallbackController != null) {
            mCallbackController.destroy();
            mCallbackController = null;
        }
        ApplicationStatus.unregisterApplicationStateListener(mApplicationStateListener);
    }

    private void notifyVoiceAvailabilityImpacted() {
        for (Observer o : mObservers) {
            o.onVoiceAvailabilityImpacted();
        }
    }

    /**
     * Instantiated when a voice search is performed to monitor the web contents for a navigation to
     * be started so we can notify the render frame that a user gesture has been performed. This
     * allows autoplay of the voice response for search results.
     */
    private final class VoiceSearchWebContentsObserver extends WebContentsObserver {
        public VoiceSearchWebContentsObserver(WebContents webContents) {
            super(webContents);
        }

        /**
         * Forces the user gesture flag to be set on a render frame if the URL being navigated to is
         * a SRP.
         *
         * @param url The URL for the navigation that started, so we can ensure that what we're
         *     navigating to is actually a SRP.
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

    /** Callback for when we receive voice search results after initiating voice recognition. */
    class VoiceRecognitionCompleteCallback implements WindowAndroid.IntentCallback {
        @VoiceInteractionSource private final int mSource;

        private boolean mCallbackComplete;

        public VoiceRecognitionCompleteCallback(@VoiceInteractionSource int source) {
            mSource = source;
        }

        // WindowAndroid.IntentCallback implementation:
        @Override
        public void onIntentCompleted(int resultCode, Intent data) {
            if (mCallbackComplete) {
                return;
            }

            mCallbackComplete = true;
            if (resultCode == Activity.RESULT_CANCELED) {
                recordVoiceSearchDismissedEvent(mSource);
                mDelegate.notifyVoiceRecognitionCanceled();
                return;
            }
            if (resultCode != Activity.RESULT_OK || data.getExtras() == null) {
                recordVoiceSearchFailureEvent(mSource);
                mDelegate.notifyVoiceRecognitionCanceled();
                return;
            }

            recordSuccessMetrics(mSource);
            handleTranscriptionResult(data);
        }

        /**
         * Processes the transcription results within the given Intent, potentially initiating a
         * search or navigation.
         *
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
                recordVoiceSearchResult(false);
                return;
            }

            String topResultQuery = topResult.getMatch();
            if (TextUtils.isEmpty(topResultQuery)) {
                recordVoiceSearchResult(false);
                return;
            }

            recordVoiceSearchResult(true);
            recordVoiceSearchConfidenceValue(topResult.getConfidence());

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
            AutocompleteMatch match = AutocompleteCoordinator.classify(profile, topResultQuery);

            String url;
            if (match == null || match.isSearchSuggestion()) {
                url =
                        TemplateUrlServiceFactory.getForProfile(profile)
                                .getUrlForVoiceSearchQuery(topResultQuery)
                                .getSpec();
            } else {
                url = match.getUrl().getSpec();
            }

            mDelegate.loadUrlFromVoice(url);
        }
    }

    /** Convert the android voice intent bundle to a list of result objects. */
    @VisibleForTesting
    protected List<VoiceResult> convertBundleToVoiceResults(Bundle extras) {
        if (extras == null) return null;

        ArrayList<String> strings = extras.getStringArrayList(RecognizerIntent.EXTRA_RESULTS);
        float[] confidences = extras.getFloatArray(RecognizerIntent.EXTRA_CONFIDENCE_SCORES);

        if (strings == null || confidences == null) return null;
        if (strings.size() != confidences.length) return null;

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
                match = AutocompleteCoordinator.classify(mProfileSupplier.get(), culledString);
            }

            String urlOrSearchQuery;
            if (match == null || match.isSearchSuggestion()) {
                urlOrSearchQuery = strings.get(i);
            } else {
                urlOrSearchQuery = culledString;
            }
            results.add(new VoiceResult(urlOrSearchQuery, confidences[i]));
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

        if (!VoiceRecognitionUtil.isVoiceSearchPermittedByPolicy(/* strictPolicyCheck= */ true)) {
            mDelegate.notifyVoiceRecognitionCanceled();
            return;
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
     * <p>In a situation when permissions can't be requested anymore, or have been requested and the
     * result was a denial without an option to request them again, voice functionality will become
     * unavailable.
     *
     * @param activity The current {@link Activity} that we're requesting permission for.
     * @param windowAndroid Used to request audio permissions from the Android system.
     * @param source The source of the mic button click, used to record metrics.
     * @return Whether audio permissions are granted.
     */
    private boolean ensureAudioPermissionGranted(
            Activity activity, WindowAndroid windowAndroid, @VoiceInteractionSource int source) {
        if (windowAndroid.hasPermission(Manifest.permission.RECORD_AUDIO)) {
            return true;
        }
        // If we don't have permission and also can't ask, then there's no more work left other
        // than telling the delegate to update the mic state.
        if (!windowAndroid.canRequestPermission(Manifest.permission.RECORD_AUDIO)) {
            notifyVoiceAvailabilityImpacted();
            return false;
        }

        PermissionCallback callback =
                (permissions, grantResults) -> {
                    if (grantResults.length != 1) {
                        mDelegate.notifyVoiceRecognitionCanceled();
                        return;
                    }

                    if (grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                        // Don't record granted permission here, it will get logged from
                        // within startSystemForVoiceSearch call.
                        startSystemForVoiceSearch(activity, windowAndroid, source);
                    } else if (!windowAndroid.canRequestPermission(
                            Manifest.permission.RECORD_AUDIO)) {
                        notifyVoiceAvailabilityImpacted();
                        mDelegate.notifyVoiceRecognitionCanceled();
                    } else {
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
        intent.putExtra(
                RecognizerIntent.EXTRA_CALLING_PACKAGE,
                activity.getComponentName().flattenToString());
        intent.putExtra(RecognizerIntent.EXTRA_WEB_SEARCH_ONLY, true);

        if (!showSpeechRecognitionIntent(windowAndroid, intent, source)) {
            // Requery whether or not the recognition intent can be handled.
            isRecognitionIntentPresent(false);
            notifyVoiceAvailabilityImpacted();
            recordVoiceSearchFailureEvent(source);

            return false;
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
     *     omnibox.
     * @return True if showing the {@link Intent} was successful.
     */
    private boolean showSpeechRecognitionIntent(
            WindowAndroid windowAndroid, Intent intent, @VoiceInteractionSource int source) {
        recordVoiceSearchStartEvent(source);
        return windowAndroid.showCancelableIntent(
                        intent,
                        new VoiceRecognitionCompleteCallback(source),
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
                ApplicationStatus.registerApplicationStateListener(mApplicationStateListener);
                mRegisteredActivityStateListener = true;
            }
        }

        return mIsVoiceSearchEnabledCached;
    }

    private void onApplicationStateChange(@ApplicationState int newState) {
        if (newState == ApplicationState.HAS_PAUSED_ACTIVITIES) {
            mIsVoiceSearchEnabledCached = null;
        }
    }

    /** Start tracking query duration by capturing when it started */
    private void startTrackingQueryDuration() {
        mQueryStartTimeMs = SystemClock.elapsedRealtime();
    }

    /** Record metrics that are only logged for successful intent responses. */
    @VisibleForTesting
    protected void recordSuccessMetrics(@VoiceInteractionSource int source) {
        // Defensive check to guard against onIntentResult being called more than once. This only
        // happens with assistant experiments. See crbug.com/1116927 for details.
        if (mQueryStartTimeMs == null) return;
        mQueryStartTimeMs = null;

        recordVoiceSearchFinishEvent(source);
    }

    /**
     * Records the source of a voice search initiation.
     *
     * @param source The source of the voice search, such as NTP or omnibox. Values taken from the
     *     enum VoiceInteractionEventSource in enums.xml.
     */
    @VisibleForTesting
    protected void recordVoiceSearchStartEvent(@VoiceInteractionSource int source) {
        RecordHistogram.recordEnumeratedHistogram(
                "VoiceInteraction.StartEventSource", source, VoiceInteractionSource.NUM_ENTRIES);
    }

    /**
     * Records the source of a successful voice search completion.
     *
     * @param source The source of the voice search, such as NTP or omnibox. Values taken from the
     *     enum VoiceInteractionEventSource in enums.xml.
     */
    @VisibleForTesting
    protected void recordVoiceSearchFinishEvent(@VoiceInteractionSource int source) {
        RecordHistogram.recordEnumeratedHistogram(
                "VoiceInteraction.FinishEventSource", source, VoiceInteractionSource.NUM_ENTRIES);
    }

    /**
     * Records the source of a dismissed voice search.
     *
     * @param source The source of the voice search, such as NTP or omnibox. Values taken from the
     *     enum VoiceInteractionEventSource in enums.xml.
     */
    @VisibleForTesting
    protected void recordVoiceSearchDismissedEvent(@VoiceInteractionSource int source) {
        RecordHistogram.recordEnumeratedHistogram(
                "VoiceInteraction.DismissedEventSource",
                source,
                VoiceInteractionSource.NUM_ENTRIES);
    }

    /**
     * Records the source of a failed voice search.
     *
     * @param source The source of the voice search, such as NTP or omnibox. Values taken from the
     *     enum VoiceInteractionEventSource in enums.xml.
     */
    @VisibleForTesting
    protected void recordVoiceSearchFailureEvent(@VoiceInteractionSource int source) {
        RecordHistogram.recordEnumeratedHistogram(
                "VoiceInteraction.FailureEventSource", source, VoiceInteractionSource.NUM_ENTRIES);
    }

    /**
     * Records the result of a voice search.
     *
     * @param result The result of a voice search, true if results were successfully returned.
     */
    @VisibleForTesting
    protected void recordVoiceSearchResult(boolean result) {
        RecordHistogram.recordBooleanHistogram("VoiceInteraction.VoiceSearchResult", result);
    }

    /**
     * Records the voice search confidence value as a percentage, instead of the 0.0 to 1.0 range we
     * receive.
     *
     * @param value The voice search confidence value we received from 0.0 to 1.0.
     */
    @VisibleForTesting
    protected void recordVoiceSearchConfidenceValue(float value) {
        int percentage = Math.round(value * 100f);
        RecordHistogram.recordPercentageHistogram(
                "VoiceInteraction.VoiceResultConfidenceValue", percentage);
    }

    /**
     * Calls into {@link VoiceRecognitionUtil} to determine whether or not the {@link
     * RecognizerIntent#ACTION_RECOGNIZE_SPEECH} {@link Intent} is handled by any {@link
     * android.app.Activity}s in the system.
     *
     * @param useCachedValue Whether or not to use the cached value from a previous result.
     * @return {@code true} if recognition is supported. {@code false} otherwise.
     */
    @VisibleForTesting
    protected static boolean isRecognitionIntentPresent(boolean useCachedValue) {
        return VoiceRecognitionUtil.isRecognitionIntentPresent(useCachedValue);
    }

    /** Sets the start time for testing. */
    void setQueryStartTimeForTesting(Long queryStartTimeMs) {
        mQueryStartTimeMs = queryStartTimeMs;
    }
}
