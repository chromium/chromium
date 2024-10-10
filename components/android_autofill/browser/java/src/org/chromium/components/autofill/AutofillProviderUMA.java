// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import android.content.Context;

import androidx.annotation.IntDef;

import org.chromium.autofill.mojom.SubmissionSource;
import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.concurrent.TimeUnit;

/**
 * The class for AutofillProvider-related UMA. Note that most of the concrete histogram
 * names include "WebView"; when this class was originally developed it was WebView-specific,
 * and when generalizing it we did not change these names to maintain continuity when
 * analyzing the histograms.
 */
public class AutofillProviderUMA {
    // Records whether the Autofill service is enabled or not.
    public static final String UMA_AUTOFILL_ENABLED = "Autofill.WebView.Enabled";

    // Records whether the Autofill provider is created by activity context or not.
    public static final String UMA_AUTOFILL_CREATED_BY_ACTIVITY_CONTEXT =
            "Autofill.WebView.CreatedByActivityContext";

    // Records what happened in an autofill session.
    public static final String UMA_AUTOFILL_AUTOFILL_SESSION = "Autofill.WebView.AutofillSession";
    public static final String UMA_AUTOFILL_AUTOFILL_SESSION_WITH_BOTTOM_SHEET =
            "Autofill.WebView.AutofillSessionWithBottomSheet";
    // The possible value of UMA_AUTOFILL_AUTOFILL_SESSION.
    public static final int SESSION_UNKNOWN = 0;
    public static final int NO_STRUCTURE_PROVIDED = 1;
    public static final int NO_SUGGESTION_USER_CHANGE_FORM_FORM_SUBMITTED = 2;
    public static final int NO_SUGGESTION_USER_CHANGE_FORM_NO_FORM_SUBMITTED = 3;
    public static final int NO_SUGGESTION_USER_NOT_CHANGE_FORM_FORM_SUBMITTED = 4;
    public static final int NO_SUGGESTION_USER_NOT_CHANGE_FORM_NO_FORM_SUBMITTED = 5;
    public static final int USER_SELECT_SUGGESTION_USER_CHANGE_FORM_FORM_SUBMITTED = 6;
    public static final int USER_SELECT_SUGGESTION_USER_CHANGE_FORM_NO_FORM_SUBMITTED = 7;
    public static final int USER_SELECT_SUGGESTION_USER_NOT_CHANGE_FORM_FORM_SUBMITTED = 8;
    public static final int USER_SELECT_SUGGESTION_USER_NOT_CHANGE_FORM_NO_FORM_SUBMITTED = 9;
    public static final int USER_NOT_SELECT_SUGGESTION_USER_CHANGE_FORM_FORM_SUBMITTED = 10;
    public static final int USER_NOT_SELECT_SUGGESTION_USER_CHANGE_FORM_NO_FORM_SUBMITTED = 11;
    public static final int USER_NOT_SELECT_SUGGESTION_USER_NOT_CHANGE_FORM_FORM_SUBMITTED = 12;
    public static final int USER_NOT_SELECT_SUGGESTION_USER_NOT_CHANGE_FORM_NO_FORM_SUBMITTED = 13;
    public static final int NO_SUGGESTION_FORM_AUTOFILLED = 19; // Error state.
    public static final int NO_SUGGESTION_FORM_AUTOFILLED_AWG = 20; // Error state.
    public static final int NO_SUGGESTION_FORM_AUTOFILLED_SAMSUNG = 21; // Error state.
    public static final int NO_SUGGESTION_FORM_AUTOFILLED_LAST_PASS = 22; // Error state.
    public static final int NO_SUGGESTION_FORM_AUTOFILLED_DASHLANE = 23; // Error state.
    public static final int NO_SUGGESTION_FORM_AUTOFILLED_1PASSWORD = 24; // Error state.
    public static final int NO_SUGGESTION_FORM_AUTOFILLED_BITWARDEN = 25; // Error state.
    public static final int AUTOFILL_SESSION_HISTOGRAM_COUNT = 26;

    // The possible values for the server prediction availability.
    public static final String UMA_AUTOFILL_SERVER_PREDICTION_AVAILABILITY =
            "Autofill.WebView.ServerPredicton.PredictionAvailability";
    public static final int SERVER_PREDICTION_NOT_AVAILABLE = 0;
    public static final int SERVER_PREDICTION_AVAILABLE_ON_SESSION_STARTS = 1;
    public static final int SERVER_PREDICTION_AVAILABLE_AFTER_SESSION_STARTS = 2;
    public static final int SERVER_PREDICTION_AVAILABLE_COUNT = 3;

    // The possible values for the AwG suggestion availability.
    public static final String UMA_AUTOFILL_AWG_SUGGESTION_AVAILABILITY =
            "Autofill.WebView.ServerPrediction.AwGSuggestionAvailability";
    public static final int AWG_NO_SUGGESTION = 0;
    public static final int AWG_HAS_SUGGESTION_NO_AUTOFILL = 1;
    public static final int AWG_HAS_SUGGESTION_AUTOFILLED = 2;
    public static final int AWG_SUGGESTION_AVAILABLE_COUNT = 3;

    public static final String UMA_AUTOFILL_VALID_SERVER_PREDICTION =
            "Autofill.WebView.ServerPredicton.HasValidServerPrediction";

    public static final String UMA_AUTOFILL_SUBMISSION_SOURCE = "Autofill.WebView.SubmissionSource";
    // The possible value of UMA_AUTOFILL_SUBMISSION_SOURCE.
    public static final int SAME_DOCUMENT_NAVIGATION = 0;
    public static final int XHR_SUCCEEDED = 1;
    public static final int FRAME_DETACHED = 2;
    // public static final int DEPRECATED_DOM_MUTATION_AFTER_XHR = 3;
    public static final int PROBABLY_FORM_SUBMITTED = 4;
    public static final int FORM_SUBMISSION = 5;
    public static final int DOM_MUTATION_AFTER_AUTOFILL = 6;
    public static final int SUBMISSION_SOURCE_HISTOGRAM_COUNT = 7;

    // The million seconds from the autofill session starting to the suggestion being displayed.
    public static final String UMA_AUTOFILL_SUGGESTION_TIME = "Autofill.WebView.SuggestionTime";

    // The expected time range of time is from 10ms to 2 seconds, and 50 buckets is sufficient.
    private static final long MIN_TIME_MILLIS = 10;
    private static final long MAX_TIME_MILLIS = TimeUnit.SECONDS.toMillis(2);
    private static final int NUM_OF_BUCKETS = 50;

    // The package name of the autofill provider. To add a new provider:
    // 1) Add a String for the provider's package name
    // 2) Add an int equal to Provider.MAX_VALUE for the provider in the Provider interface.
    // 3) Increment Provider.MAX_VALUE.
    // 4) Update tools/metrics/histograms/enums.xml with the new entry.
    // 5) Look for switch statements that uses those values and update them accordingly.
    private static final String UMA_AUTOFILL_PROVIDER = "Autofill.WebView.Provider.PackageName";
    private static final String AWG_PACKAGE_NAME = "com.google.android.gms";
    private static final String SAMSUNG_PASS_PACKAGE_NAME =
            "com.samsung.android.samsungpassautofill";
    private static final String LASTPASS_PACKAGE_NAME = "com.lastpass.lpandroid";
    private static final String DASHLANE_PACKAGE_NAME = "com.dashlane";
    private static final String ONE_PASSWORD_PACKAGE_NAME = "com.onepassword.android";
    private static final String BITWARDEN_PACKAGE_NAME = "com.x8bit.bitwarden";

    @IntDef({
        Provider.UNKNOWN,
        Provider.AWG,
        Provider.SAMSUNG_PASS,
        Provider.LAST_PASS,
        Provider.DASHLANE,
        Provider.ONE_PASSWORD,
        Provider.BITWARDEN,
        Provider.MAX_VALUE
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface Provider {
        int UNKNOWN = 0;
        int AWG = 1;
        int SAMSUNG_PASS = 2;
        int LAST_PASS = 3;
        int DASHLANE = 4;
        int ONE_PASSWORD = 5;
        int BITWARDEN = 6;
        int MAX_VALUE = 7;
    }

    private static void recordTimesHistogram(String name, long durationMillis) {
        RecordHistogram.recordCustomTimesHistogram(
                name, durationMillis, MIN_TIME_MILLIS, MAX_TIME_MILLIS, NUM_OF_BUCKETS);
    }

    private static class SessionRecorder {
        // These values are recorded as UMAs - do not change them.
        public static final int EVENT_VIRTUAL_STRUCTURE_PROVIDED = 1 << 0;
        public static final int EVENT_SUGGESTION_DISPLAYED = 1 << 1;
        public static final int EVENT_FORM_AUTOFILLED = 1 << 2;
        public static final int EVENT_USER_CHANGED_FIELD_VALUE = 1 << 3;
        public static final int EVENT_USER_CHANGED_AUTOFILLED_FIELD = 1 << 4;
        public static final int EVENT_FIELD_CHANGED_VISIBILITY = 1 << 5;
        public static final int EVENT_FORM_SUBMITTED = 1 << 6;
        public static final int EVENT_BOTTOM_SHEET_SHOWN = 1 << 7;

        private Long mSuggestionTimeMillis;

        public void record(int event) {
            // Do not record any event until we get EVENT_VIRTUAL_STRUCTURE_PROVIDED, which makes
            // the following events meaningful.
            if (event != EVENT_VIRTUAL_STRUCTURE_PROVIDED && mState == 0) return;
            if (EVENT_USER_CHANGED_AUTOFILLED_FIELD == event) {
                event |= EVENT_USER_CHANGED_FIELD_VALUE;
            }
            mState |= event;
        }

        public void setSuggestionTimeMillis(long suggestionTimeMillis) {
            // Only record first suggestion.
            if (mSuggestionTimeMillis == null) {
                mSuggestionTimeMillis = suggestionTimeMillis;
            }
        }

        public void recordHistogram(@Provider int currentProvider) {
            final int sessionValue = toUMAAutofillSessionValue(currentProvider);
            RecordHistogram.recordEnumeratedHistogram(
                    UMA_AUTOFILL_AUTOFILL_SESSION, sessionValue, AUTOFILL_SESSION_HISTOGRAM_COUNT);
            // If a bottom sheet was shown, we record an additional, separate metric for it.
            if ((mState & EVENT_BOTTOM_SHEET_SHOWN) != 0) {
                RecordHistogram.recordEnumeratedHistogram(
                        UMA_AUTOFILL_AUTOFILL_SESSION_WITH_BOTTOM_SHEET,
                        sessionValue,
                        AUTOFILL_SESSION_HISTOGRAM_COUNT);
            }
            if (mSuggestionTimeMillis != null) {
                recordTimesHistogram(UMA_AUTOFILL_SUGGESTION_TIME, mSuggestionTimeMillis);
            }
            if (!mServerPredictionAvailable) {
                RecordHistogram.recordEnumeratedHistogram(
                        UMA_AUTOFILL_SERVER_PREDICTION_AVAILABILITY,
                        SERVER_PREDICTION_NOT_AVAILABLE,
                        SERVER_PREDICTION_AVAILABLE_COUNT);
            }
        }

        public void onServerTypeAvailable(FormData formData, boolean afterSessionStarted) {
            mServerPredictionAvailable = true;
            RecordHistogram.recordEnumeratedHistogram(
                    UMA_AUTOFILL_SERVER_PREDICTION_AVAILABILITY,
                    afterSessionStarted
                            ? SERVER_PREDICTION_AVAILABLE_AFTER_SESSION_STARTS
                            : SERVER_PREDICTION_AVAILABLE_ON_SESSION_STARTS,
                    SERVER_PREDICTION_AVAILABLE_COUNT);
            if (formData != null) {
                boolean hasValidServerData = false;
                for (FormFieldData fieldData : formData.mFields) {
                    if (!fieldData.getServerType().equals("NO_SERVER_DATA")) {
                        hasValidServerData = true;
                        break;
                    }
                }
                RecordHistogram.recordBooleanHistogram(
                        UMA_AUTOFILL_VALID_SERVER_PREDICTION, hasValidServerData);
            }
        }

        private int toUMAAutofillSessionValue(@Provider int currentProvider) {
            // No other events are recorded until EVENT_VIRTUAL_STRUCTURE_PROVIDED is recorded.
            if ((mState & EVENT_VIRTUAL_STRUCTURE_PROVIDED) == 0) {
                return NO_STRUCTURE_PROVIDED;
            }
            // We know that the structure is provided from here on.
            // Only the below four events are considered for translating the events to
            // an AUTOFILL_SESSION record.
            int state =
                    mState
                            & (EVENT_SUGGESTION_DISPLAYED
                                    | EVENT_FORM_AUTOFILLED
                                    | EVENT_USER_CHANGED_FIELD_VALUE
                                    | EVENT_FORM_SUBMITTED);
            if ((state & EVENT_SUGGESTION_DISPLAYED) == 0) {
                // No suggestions were shown and hence one cannot autofill.
                if ((state & EVENT_FORM_AUTOFILLED) != 0) { // 4 states
                    switch (currentProvider) {
                        case Provider.AWG:
                            return NO_SUGGESTION_FORM_AUTOFILLED_AWG; // Error state.
                        case Provider.SAMSUNG_PASS:
                            return NO_SUGGESTION_FORM_AUTOFILLED_SAMSUNG; // Error state.
                        case Provider.LAST_PASS:
                            return NO_SUGGESTION_FORM_AUTOFILLED_LAST_PASS; // Error state.
                        case Provider.DASHLANE:
                            return NO_SUGGESTION_FORM_AUTOFILLED_DASHLANE; // Error state.
                        case Provider.ONE_PASSWORD:
                            return NO_SUGGESTION_FORM_AUTOFILLED_1PASSWORD; // Error state.
                        case Provider.BITWARDEN:
                            return NO_SUGGESTION_FORM_AUTOFILLED_BITWARDEN; // Error state.
                        default:
                            return NO_SUGGESTION_FORM_AUTOFILLED; // Error state.
                    }
                }
                if (state == 0) { // 1 state
                    return NO_SUGGESTION_USER_NOT_CHANGE_FORM_NO_FORM_SUBMITTED;
                }
                if (state == EVENT_USER_CHANGED_FIELD_VALUE) { // 1 state
                    return NO_SUGGESTION_USER_CHANGE_FORM_NO_FORM_SUBMITTED;
                }
                if (state == EVENT_FORM_SUBMITTED) { // 1 state
                    return NO_SUGGESTION_USER_NOT_CHANGE_FORM_FORM_SUBMITTED;
                }
                if (state == (EVENT_USER_CHANGED_FIELD_VALUE | EVENT_FORM_SUBMITTED)) { // 1 state
                    return NO_SUGGESTION_USER_CHANGE_FORM_FORM_SUBMITTED;
                }
                return SESSION_UNKNOWN; // Shouldn't reach this state.
            }
            // We know that below suggestions were shown.
            state ^= EVENT_SUGGESTION_DISPLAYED;
            if ((state & EVENT_FORM_AUTOFILLED) == 0) {
                if (state == 0) { // 1 state
                    return USER_NOT_SELECT_SUGGESTION_USER_NOT_CHANGE_FORM_NO_FORM_SUBMITTED;
                }
                if (state == EVENT_USER_CHANGED_FIELD_VALUE) { // 1 state
                    return USER_NOT_SELECT_SUGGESTION_USER_CHANGE_FORM_NO_FORM_SUBMITTED;
                }
                if (state == EVENT_FORM_SUBMITTED) { // 1 state
                    return USER_NOT_SELECT_SUGGESTION_USER_NOT_CHANGE_FORM_FORM_SUBMITTED;
                }
                if (state == (EVENT_USER_CHANGED_FIELD_VALUE | EVENT_FORM_SUBMITTED)) { // 1 state
                    return USER_NOT_SELECT_SUGGESTION_USER_CHANGE_FORM_FORM_SUBMITTED;
                }
                return SESSION_UNKNOWN; // Shouldn't reach this state.
            }
            // We know that below the form was autofilled.
            state ^= EVENT_FORM_AUTOFILLED;
            if (state == 0) { // 1 state
                return USER_SELECT_SUGGESTION_USER_NOT_CHANGE_FORM_NO_FORM_SUBMITTED;
            }
            if (state == EVENT_USER_CHANGED_FIELD_VALUE) { // 1 state
                return USER_SELECT_SUGGESTION_USER_CHANGE_FORM_NO_FORM_SUBMITTED;
            }
            if (state == EVENT_FORM_SUBMITTED) { // 1 state
                return USER_SELECT_SUGGESTION_USER_NOT_CHANGE_FORM_FORM_SUBMITTED;
            }
            if (state == (EVENT_USER_CHANGED_FIELD_VALUE | EVENT_FORM_SUBMITTED)) { // 1 state
                return USER_SELECT_SUGGESTION_USER_CHANGE_FORM_FORM_SUBMITTED;
            }
            return SESSION_UNKNOWN; // Shouldn't reach this state.
        }

        private int mState;

        // Indicates whether the server prediction arrives.
        private boolean mServerPredictionAvailable;
    }

    /**
     * The class to record Autofill.WebView.ServerPrediction.AwGSuggestion, is only instantiated
     * when the Android platform AutofillService is AwG, This will give us more actual result in A/B
     * experiment while only AwG supports the server prediction.
     */
    private static class ServerPredictionRecorder {
        private boolean mHasSuggestions;
        private boolean mAutofilled;
        private boolean mRecorded;

        public void onSuggestionDisplayed() {
            mHasSuggestions = true;
        }

        public void onAutofill() {
            mAutofilled = true;
        }

        public void recordHistograms() {
            if (mRecorded) return;
            mRecorded = true;
            int sample = AWG_NO_SUGGESTION;
            if (mHasSuggestions) {
                sample =
                        mAutofilled
                                ? AWG_HAS_SUGGESTION_AUTOFILLED
                                : AWG_HAS_SUGGESTION_NO_AUTOFILL;
            }
            RecordHistogram.recordEnumeratedHistogram(
                    UMA_AUTOFILL_AWG_SUGGESTION_AVAILABILITY,
                    sample,
                    AWG_SUGGESTION_AVAILABLE_COUNT);
        }
    }

    private SessionRecorder mRecorder;
    private Boolean mAutofillDisabledOnSessionStart;

    private final boolean mIsAwGCurrentAutofillService;
    private ServerPredictionRecorder mServerPredictionRecorder;

    private final @Provider int mCurrentProvider;

    public AutofillProviderUMA(
            Context context, boolean isAwGCurrentAutofillService, String packageName) {
        mCurrentProvider = getCurrentProvider(packageName);
        RecordHistogram.recordBooleanHistogram(
                UMA_AUTOFILL_CREATED_BY_ACTIVITY_CONTEXT,
                ContextUtils.activityFromContext(context) != null);
        mIsAwGCurrentAutofillService = isAwGCurrentAutofillService;
    }

    public void onFormSubmitted(int submissionSource) {
        if (mRecorder != null) mRecorder.record(SessionRecorder.EVENT_FORM_SUBMITTED);
        recordSession();
        // TODO(crbug.com/40933028): Consider moving the call to the ServerPredictionRecorder
        // into recordSession. Is it unclear why this is only recorded on form submission.
        if (mServerPredictionRecorder != null) mServerPredictionRecorder.recordHistograms();
        // We record this no matter autofill service is disabled or not.
        RecordHistogram.recordEnumeratedHistogram(
                UMA_AUTOFILL_SUBMISSION_SOURCE,
                toUMASubmissionSource(submissionSource),
                SUBMISSION_SOURCE_HISTOGRAM_COUNT);
    }

    public void onSessionStarted(boolean autofillDisabled) {
        // Record autofill status once per instance and only if user triggers the autofill.
        if (mAutofillDisabledOnSessionStart == null
                || mAutofillDisabledOnSessionStart.booleanValue() != autofillDisabled) {
            RecordHistogram.recordBooleanHistogram(UMA_AUTOFILL_ENABLED, !autofillDisabled);
            mAutofillDisabledOnSessionStart = autofillDisabled;
        }

        if (mRecorder != null) recordSession();
        mRecorder = new SessionRecorder();
        if (mIsAwGCurrentAutofillService) {
            mServerPredictionRecorder = new ServerPredictionRecorder();
        }
    }

    public void onVirtualStructureProvided() {
        if (mRecorder != null) mRecorder.record(SessionRecorder.EVENT_VIRTUAL_STRUCTURE_PROVIDED);
    }

    public void onSuggestionDisplayed(long suggestionTimeMillis) {
        if (mRecorder != null) {
            mRecorder.record(SessionRecorder.EVENT_SUGGESTION_DISPLAYED);
            mRecorder.setSuggestionTimeMillis(suggestionTimeMillis);
        }
        if (mServerPredictionRecorder != null) mServerPredictionRecorder.onSuggestionDisplayed();
    }

    public void onAutofill() {
        if (mRecorder != null) mRecorder.record(SessionRecorder.EVENT_FORM_AUTOFILLED);
        if (mServerPredictionRecorder != null) mServerPredictionRecorder.onAutofill();
    }

    public void onBottomSheetShown() {
        if (mRecorder == null) return;
        // The virtual structure event is provided prior to session start for sessions with a bottom
        // sheet. Therefore we record it here manually.
        mRecorder.record(SessionRecorder.EVENT_VIRTUAL_STRUCTURE_PROVIDED);
        mRecorder.record(SessionRecorder.EVENT_BOTTOM_SHEET_SHOWN);
    }

    public void onUserChangeFieldValue(boolean isPreviouslyAutofilled) {
        if (mRecorder == null) return;
        if (isPreviouslyAutofilled) {
            mRecorder.record(SessionRecorder.EVENT_USER_CHANGED_AUTOFILLED_FIELD);
        } else {
            mRecorder.record(SessionRecorder.EVENT_USER_CHANGED_FIELD_VALUE);
        }
    }

    public void onFieldChangedVisibility() {
        if (mRecorder != null) {
            mRecorder.record(SessionRecorder.EVENT_FIELD_CHANGED_VISIBILITY);
        }
    }

    /**
     * Invoked when the server query was done or has arrived when the autofill session starts.
     *
     * @param formData the form of the current session, is null if the query failed.
     * @param afterSessionStarted true if the server type predication arrive after the session
     *     starts.
     */
    public void onServerTypeAvailable(FormData formData, boolean afterSessionStarted) {
        mRecorder.onServerTypeAvailable(formData, afterSessionStarted);
    }

    private static @Provider int getCurrentProvider(String packageName) {
        switch (packageName) {
            case AWG_PACKAGE_NAME:
                return Provider.AWG;
            case SAMSUNG_PASS_PACKAGE_NAME:
                return Provider.SAMSUNG_PASS;
            case LASTPASS_PACKAGE_NAME:
                return Provider.LAST_PASS;
            case DASHLANE_PACKAGE_NAME:
                return Provider.DASHLANE;
            case ONE_PASSWORD_PACKAGE_NAME:
                return Provider.ONE_PASSWORD;
            case BITWARDEN_PACKAGE_NAME:
                return Provider.BITWARDEN;
            default:
                return Provider.UNKNOWN;
        }
    }

    static void logCurrentProvider(String packageName) {
        recordUmaAutofillProvider(getCurrentProvider(packageName));
    }

    /**
     * Records the session-related Autofill metrics, i.e. the witnessed Autofill events and the
     * AUTOFILL_SESSION UMA.
     *
     * <p>After recording, it resets the SessionRecorder. Calling it again is a no-op until a new
     * session has been started.
     */
    public void recordSession() {
        if (mAutofillDisabledOnSessionStart != null
                && !mAutofillDisabledOnSessionStart.booleanValue()
                && mRecorder != null) {
            mRecorder.recordHistogram(mCurrentProvider);
        }
        mRecorder = null;
    }

    private static void recordUmaAutofillProvider(@Provider int autofillProvider) {
        RecordHistogram.recordEnumeratedHistogram(
                UMA_AUTOFILL_PROVIDER, autofillProvider, Provider.MAX_VALUE);
    }

    private int toUMASubmissionSource(int source) {
        switch (source) {
            case SubmissionSource.SAME_DOCUMENT_NAVIGATION:
                return SAME_DOCUMENT_NAVIGATION;
            case SubmissionSource.XHR_SUCCEEDED:
                return XHR_SUCCEEDED;
            case SubmissionSource.FRAME_DETACHED:
                return FRAME_DETACHED;
            case SubmissionSource.PROBABLY_FORM_SUBMITTED:
                return PROBABLY_FORM_SUBMITTED;
            case SubmissionSource.FORM_SUBMISSION:
                return FORM_SUBMISSION;
            case SubmissionSource.DOM_MUTATION_AFTER_AUTOFILL:
                return DOM_MUTATION_AFTER_AUTOFILL;
            default:
                return SUBMISSION_SOURCE_HISTOGRAM_COUNT;
        }
    }
}
