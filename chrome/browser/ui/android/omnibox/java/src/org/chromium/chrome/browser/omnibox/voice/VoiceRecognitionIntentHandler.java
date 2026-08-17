// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.voice;

import android.Manifest;
import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.speech.RecognizerIntent;
import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.base.WindowAndroid.IntentCallback;
import org.chromium.ui.permissions.PermissionCallback;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;

/**
 * Handles the platform-specific speech recognition intent. Encapsulates permission checks, intent
 * launching, result parsing, and metrics.
 */
@NullMarked
public class VoiceRecognitionIntentHandler {

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
        VoiceInteractionSource.TOOLBAR,
        VoiceInteractionSource.COMPOSEBOX
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface VoiceInteractionSource {
        int OMNIBOX = 0;
        int NTP = 1;
        int SEARCH_WIDGET = 2;
        int TASKS_SURFACE = 3;
        int TOOLBAR = 4;
        int COMPOSEBOX = 5;

        // Be sure to also update enums.xml when updating these values.
        int NUM_ENTRIES = 6;
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

    public interface RecognitionCallback {
        /** Called when transcription completes successfully with results. */
        void onCompleted(List<VoiceResult> results);

        /** Called when the speech recognition fails or is canceled. */
        void onCanceled();

        /** Called when voice availability is impacted (e.g. permanent permission denial). */
        void onAvailabilityImpacted();
    }

    private final WindowAndroid mWindowAndroid;
    private @Nullable Long mQueryStartTimeMs;
    private @Nullable Boolean mIsVoiceSearchEnabledCached;
    private boolean mRegisteredActivityStateListener;
    private final ApplicationStatus.ApplicationStateListener mApplicationStateListener =
            this::onApplicationStateChange;

    public VoiceRecognitionIntentHandler(WindowAndroid windowAndroid) {
        mWindowAndroid = windowAndroid;
    }

    public void destroy() {
        if (mRegisteredActivityStateListener) {
            ApplicationStatus.unregisterApplicationStateListener(mApplicationStateListener);
            mRegisteredActivityStateListener = false;
        }
    }

    private void onApplicationStateChange(@ApplicationState int newState) {
        if (newState == ApplicationState.HAS_PAUSED_ACTIVITIES) {
            mIsVoiceSearchEnabledCached = null;
        }
    }

    /** Checks if voice search is enabled for the given window. */
    public boolean isVoiceSearchEnabled() {
        if (mWindowAndroid == null) return false;
        if (mWindowAndroid.getActivity().get() == null) return false;
        if (!VoiceRecognitionUtil.isVoiceSearchPermittedByPolicy(false)) return false;

        if (mIsVoiceSearchEnabledCached == null) {
            mIsVoiceSearchEnabledCached = VoiceRecognitionUtil.isVoiceSearchEnabled(mWindowAndroid);

            if (!mRegisteredActivityStateListener) {
                ApplicationStatus.registerApplicationStateListener(mApplicationStateListener);
                mRegisteredActivityStateListener = true;
            }
        }

        return mIsVoiceSearchEnabledCached;
    }

    /**
     * Starts the speech recognition flow.
     *
     * @param source The source of the voice search initiation, such as NTP or omnibox.
     * @param callback The callback to receive completion or cancellation events.
     */
    public void startVoiceRecognition(
            @VoiceInteractionSource int source, RecognitionCallback callback) {
        ThreadUtils.assertOnUiThread();
        startTrackingQueryDuration();

        if (mWindowAndroid == null) {
            callback.onCanceled();
            return;
        }

        Activity activity = mWindowAndroid.getActivity().get();
        if (activity == null) {
            callback.onCanceled();
            return;
        }

        if (!VoiceRecognitionUtil.isVoiceSearchPermittedByPolicy(/* strictPolicyCheck= */ true)) {
            callback.onCanceled();
            return;
        }

        startSystemForVoiceSearch(activity, source, callback);
    }

    private void startSystemForVoiceSearch(
            Activity activity, @VoiceInteractionSource int source, RecognitionCallback callback) {
        ensureAudioPermissionGranted(
                callback,
                () -> {
                    Intent intent = new Intent(RecognizerIntent.ACTION_RECOGNIZE_SPEECH);
                    intent.putExtra(
                            RecognizerIntent.EXTRA_LANGUAGE_MODEL,
                            RecognizerIntent.LANGUAGE_MODEL_WEB_SEARCH);
                    intent.putExtra(
                            RecognizerIntent.EXTRA_CALLING_PACKAGE,
                            activity.getComponentName().flattenToString());
                    intent.putExtra(RecognizerIntent.EXTRA_WEB_SEARCH_ONLY, true);

                    if (!showSpeechRecognitionIntent(intent, source, callback)) {
                        VoiceRecognitionUtil.isRecognitionIntentPresent(false);
                        recordVoiceSearchFailureEvent(source);
                        callback.onAvailabilityImpacted();
                    }
                });
    }

    /**
     * Checks if the RECORD_AUDIO permission has been granted, and requests it if it hasn't.
     *
     * @param onPermissionResolved The callback to invoke once permission is resolved.
     */
    private void ensureAudioPermissionGranted(
            RecognitionCallback callback, Runnable onPermissionGranted) {
        if (mWindowAndroid.hasPermission(Manifest.permission.RECORD_AUDIO)) {
            onPermissionGranted.run();
            return;
        }
        // If we don't have permission and also can't ask, then there's no more work left other
        // than telling the observer to update the mic state.
        if (!mWindowAndroid.canRequestPermission(Manifest.permission.RECORD_AUDIO)) {
            callback.onAvailabilityImpacted();
            return;
        }

        PermissionCallback permissionCallback =
                (permissions, grantResults) -> {
                    boolean granted =
                            grantResults.length == 1
                                    && grantResults[0] == PackageManager.PERMISSION_GRANTED;
                    if (granted) {
                        onPermissionGranted.run();
                    } else if (!mWindowAndroid.canRequestPermission(
                            Manifest.permission.RECORD_AUDIO)) {
                        callback.onAvailabilityImpacted();
                    } else {
                        callback.onCanceled();
                    }
                };
        mWindowAndroid.requestPermissions(
                new String[] {Manifest.permission.RECORD_AUDIO}, permissionCallback);
    }

    /**
     * Triggers the system-provided voice recognition intent.
     *
     * @param intent The speech recognition {@link Intent}.
     * @param source Where the request to launch this {@link Intent} originated, such as NTP or
     *     omnibox.
     * @param callback The callback to receive the results.
     * @return True if showing the {@link Intent} was successful.
     */
    private boolean showSpeechRecognitionIntent(
            Intent intent, @VoiceInteractionSource int source, RecognitionCallback callback) {
        recordVoiceSearchStartEvent(source);
        IntentCallback intentCallback =
                (resultCode, data) -> {
                    if (resultCode == Activity.RESULT_CANCELED) {
                        recordVoiceSearchDismissedEvent(source);
                        callback.onCanceled();
                        return;
                    }
                    if (resultCode != Activity.RESULT_OK || data == null) {
                        recordVoiceSearchFailureEvent(source);
                        callback.onCanceled();
                        return;
                    }

                    List<VoiceResult> results = convertBundleToVoiceResults(data.getExtras());
                    if (results == null || results.isEmpty()) {
                        recordVoiceSearchResult(false);
                        callback.onCanceled();
                        return;
                    }

                    VoiceResult topResult = results.get(0);
                    if (TextUtils.isEmpty(topResult.getMatch())) {
                        recordVoiceSearchResult(false);
                        callback.onCanceled();
                        return;
                    }

                    recordVoiceSearchResult(true);
                    recordVoiceSearchConfidenceValue(topResult.getConfidence());

                    recordSuccessMetrics(source);
                    callback.onCompleted(results);
                };

        boolean shown =
                mWindowAndroid.showCancelableIntent(
                                intent, intentCallback, R.string.voice_search_error)
                        >= 0;
        // TODO(ender): consider histograms for 'shown'.
        return shown;
    }

    /** Convert the android voice intent bundle to a list of result objects. */
    @VisibleForTesting
    static @Nullable List<VoiceResult> convertBundleToVoiceResults(@Nullable Bundle extras) {
        if (extras == null) return null;

        ArrayList<String> strings = extras.getStringArrayList(RecognizerIntent.EXTRA_RESULTS);
        float[] confidences = extras.getFloatArray(RecognizerIntent.EXTRA_CONFIDENCE_SCORES);

        if (strings == null || confidences == null) return null;
        if (strings.size() != confidences.length) return null;

        List<VoiceResult> results = new ArrayList<>();
        for (int i = 0; i < strings.size(); ++i) {
            results.add(new VoiceResult(strings.get(i), confidences[i]));
        }
        return results;
    }

    /** Start tracking query duration by capturing when it started */
    private void startTrackingQueryDuration() {
        mQueryStartTimeMs = android.os.SystemClock.elapsedRealtime();
    }

    /** Record metrics that are only logged for successful intent responses. */
    @VisibleForTesting
    void recordSuccessMetrics(@VoiceInteractionSource int source) {
        // Defensive check to guard against onIntentResult being called more than once. This only
        // happens with assistant experiments. See crbug.com/40712241 for details.
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
    void recordVoiceSearchStartEvent(@VoiceInteractionSource int source) {
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
    void recordVoiceSearchFinishEvent(@VoiceInteractionSource int source) {
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
    void recordVoiceSearchDismissedEvent(@VoiceInteractionSource int source) {
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
    void recordVoiceSearchFailureEvent(@VoiceInteractionSource int source) {
        RecordHistogram.recordEnumeratedHistogram(
                "VoiceInteraction.FailureEventSource", source, VoiceInteractionSource.NUM_ENTRIES);
    }

    /**
     * Records the result of a voice search.
     *
     * @param result The result of a voice search, true if results were successfully returned.
     */
    @VisibleForTesting
    void recordVoiceSearchResult(boolean result) {
        RecordHistogram.recordBooleanHistogram("VoiceInteraction.VoiceSearchResult", result);
    }

    /**
     * Records the voice search confidence value as a percentage, instead of the 0.0 to 1.0 range we
     * receive.
     *
     * @param value The voice search confidence value we received from 0.0 to 1.0.
     */
    @VisibleForTesting
    void recordVoiceSearchConfidenceValue(float value) {
        int percentage = Math.round(value * 100f);
        RecordHistogram.recordPercentageHistogram(
                "VoiceInteraction.VoiceResultConfidenceValue", percentage);
    }
}
