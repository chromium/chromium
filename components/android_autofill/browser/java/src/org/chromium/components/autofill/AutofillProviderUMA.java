// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import android.content.Context;
import android.os.Build;

import org.chromium.autofill.mojom.SubmissionSource;
import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;

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

    // Records whether the current autofill service is AwG.
    public static final String UMA_AUTOFILL_AWG_IS_CURRENT_SERVICE =
            "Autofill.WebView.AwGIsCurrentService";

    // Records what happened in an autofill session.
    public static final String UMA_AUTOFILL_AUTOFILL_SESSION = "Autofill.WebView.AutofillSession";
    // The possible value of UMA_AUTOFILL_AUTOFILL_SESSION.
    public static final int SESSION_UNKNOWN = 0;
    public static final int NO_CALLBACK_FORM_FRAMEWORK = 1;
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
    public static final int AUTOFILL_SESSION_HISTOGRAM_COUNT = 14;

    // The possible values for the server prediction availability.
    public static final String UMA_AUTOFILL_SERVER_PREDICTION_AVAILABILITY =
            "Autofill.WebView.ServerPredicton.PredictionAvailability";
    public static final int SERVER_PREDICTION_NOT_AVAILABLE = 0;
    public static final int SERVER_PREDICTION_AVAILABLE_ON_SESSION_STARTS = 1;
    public static final int SERVER_PREDICTION_AVAILABLE_AFTER_SESSION_STARTS = 2;
    public static final int SERVER_PREDICTION_AVAILABLE_COUNT = 3;

    // The possible values for the AwG suggestion availability.
    public static final String UMA_AUTOFILL_AWG_SUGGSTION_AVAILABILITY =
            "Autofill.WebView.ServerPrediction.AwGSuggestionAvailability";
    public static final int AWG_NO_SUGGESTION = 0;
    public static final int AWG_HAS_SUGGESTION_NO_AUTOFILL = 1;
    public static final int AWG_HAS_SUGGESTION_AUTOFILLED = 2;
    public static final int AWG_SUGGSTION_AVAILABLE_COUNT = 3;

    public static final String UMA_AUTOFILL_VALID_SERVER_PREDICTION =
            "Autofill.WebView.ServerPredicton.HasValidServerPrediction";

    // Records whether user changed autofilled field if user ever changed the form. The action isn't
    // recorded if user didn't change form at all.
    public static final String UMA_AUTOFILL_USER_CHANGED_AUTOFILLED_FIELD =
            "Autofill.WebView.UserChangedAutofilledField";

    public static final String UMA_AUTOFILL_SUBMISSION_SOURCE = "Autofill.WebView.SubmissionSource";
    // The possible value of UMA_AUTOFILL_SUBMISSION_SOURCE.
    public static final int SAME_DOCUMENT_NAVIGATION = 0;
    public static final int XHR_SUCCEEDED = 1;
    public static final int FRAME_DETACHED = 2;
    public static final int DOM_MUTATION_AFTER_XHR = 3;
    public static final int PROBABLY_FORM_SUBMITTED = 4;
    public static final int FORM_SUBMISSION = 5;
    public static final int SUBMISSION_SOURCE_HISTOGRAM_COUNT = 6;

    // The million seconds from user touched the field to the autofill session starting.
    public static final String UMA_AUTOFILL_TRIGGERING_TIME = "Autofill.WebView.TriggeringTime";

    // The million seconds from the autofill session starting to the suggestion being displayed.
    public static final String UMA_AUTOFILL_SUGGESTION_TIME = "Autofill.WebView.SuggestionTime";

    // The expected time range of time is from 10ms to 2 seconds, and 50 buckets is sufficient.
    private static final long MIN_TIME_MILLIS = 10;
    private static final long MAX_TIME_MILLIS = TimeUnit.SECONDS.toMillis(2);
    private static final int NUM_OF_BUCKETS = 50;

    private static void recordTimesHistogram(String name, long durationMillis) {
        RecordHistogram.recordCustomTimesHistogram(
                name, durationMillis, MIN_TIME_MILLIS, MAX_TIME_MILLIS, NUM_OF_BUCKETS);
    }

    private static class SessionRecorder {
        public static final int EVENT_VIRTUAL_STRUCTURE_PROVIDED = 0x1 << 0;
        public static final int EVENT_SUGGESTION_DISPLAYED = 0x1 << 1;
        public static final int EVENT_FORM_AUTOFILLED = 0x1 << 2;
        public static final int EVENT_USER_CHANGED_FIELD_VALUE = 0x1 << 3;
        public static final int EVENT_FORM_SUBMITTED = 0x1 << 4;
        public static final int EVENT_USER_CHANGED_AUTOFILLED_FIELD = 0x1 << 5;

        private Long mSuggestionTimeMillis;

        public void record(int event) {
            // Not record any event until we get EVENT_VIRTUAL_STRUCTURE_PROVIDED which makes the
            // following events meaningful.
            if (event != EVENT_VIRTUAL_STRUCTURE_PROVIDED && mState == 0) return;
            if (EVENT_USER_CHANGED_FIELD_VALUE == event && mUserChangedAutofilledField == null) {
                mUserChangedAutofilledField = Boolean.valueOf(false);
            } else if (EVENT_USER_CHANGED_AUTOFILLED_FIELD == event) {
                if (mUserChangedAutofilledField == null) {
                    mUserChangedAutofilledField = Boolean.valueOf(true);
                }
                mUserChangedAutofilledField = true;
                event = EVENT_USER_CHANGED_FIELD_VALUE;
            }
            mState |= event;
        }

        public void setSuggestionTimeMillis(long suggestionTimeMillis) {
            // Only record first suggestion.
            if (mSuggestionTimeMillis == null) {
                mSuggestionTimeMillis = Long.valueOf(suggestionTimeMillis);
            }
        }

        public void recordHistogram() {
            RecordHistogram.recordEnumeratedHistogram(UMA_AUTOFILL_AUTOFILL_SESSION,
                    toUMAAutofillSessionValue(), AUTOFILL_SESSION_HISTOGRAM_COUNT);
            // Only record if user ever changed form.
            if (mUserChangedAutofilledField != null) {
                RecordHistogram.recordBooleanHistogram(
                        UMA_AUTOFILL_USER_CHANGED_AUTOFILLED_FIELD, mUserChangedAutofilledField);
            }
            if (mSuggestionTimeMillis != null) {
                recordTimesHistogram(UMA_AUTOFILL_SUGGESTION_TIME, mSuggestionTimeMillis);
            }
            if (!mServerPredictionAvailable) {
                RecordHistogram.recordEnumeratedHistogram(
                        UMA_AUTOFILL_SERVER_PREDICTION_AVAILABILITY,
                        SERVER_PREDICTION_NOT_AVAILABLE, SERVER_PREDICTION_AVAILABLE_COUNT);
            }
        }

        public void onServerTypeAvailable(FormData formData, boolean afterSessionStarted) {
            mServerPredictionAvailable = true;
            RecordHistogram.recordEnumeratedHistogram(UMA_AUTOFILL_SERVER_PREDICTION_AVAILABILITY,
                    afterSessionStarted ? SERVER_PREDICTION_AVAILABLE_AFTER_SESSION_STARTS
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

        private int toUMAAutofillSessionValue() {
            if (mState == 0) {
                return NO_CALLBACK_FORM_FRAMEWORK;
            } else if (mState == EVENT_VIRTUAL_STRUCTURE_PROVIDED) {
                return NO_SUGGESTION_USER_NOT_CHANGE_FORM_NO_FORM_SUBMITTED;
            } else if (mState
                    == (EVENT_VIRTUAL_STRUCTURE_PROVIDED | EVENT_USER_CHANGED_FIELD_VALUE)) {
                return NO_SUGGESTION_USER_CHANGE_FORM_NO_FORM_SUBMITTED;
            } else if (mState == (EVENT_VIRTUAL_STRUCTURE_PROVIDED | EVENT_FORM_SUBMITTED)) {
                return NO_SUGGESTION_USER_NOT_CHANGE_FORM_FORM_SUBMITTED;
            } else if (mState
                    == (EVENT_VIRTUAL_STRUCTURE_PROVIDED | EVENT_USER_CHANGED_FIELD_VALUE
                            | EVENT_FORM_SUBMITTED)) {
                return NO_SUGGESTION_USER_CHANGE_FORM_FORM_SUBMITTED;
            } else if (mState
                    == (EVENT_VIRTUAL_STRUCTURE_PROVIDED | EVENT_SUGGESTION_DISPLAYED
                            | EVENT_FORM_AUTOFILLED)) {
                return USER_SELECT_SUGGESTION_USER_NOT_CHANGE_FORM_NO_FORM_SUBMITTED;
            } else if (mState
                    == (EVENT_VIRTUAL_STRUCTURE_PROVIDED | EVENT_SUGGESTION_DISPLAYED
                            | EVENT_FORM_AUTOFILLED | EVENT_FORM_SUBMITTED)) {
                return USER_SELECT_SUGGESTION_USER_NOT_CHANGE_FORM_FORM_SUBMITTED;
            } else if (mState
                    == (EVENT_VIRTUAL_STRUCTURE_PROVIDED | EVENT_SUGGESTION_DISPLAYED
                            | EVENT_FORM_AUTOFILLED | EVENT_USER_CHANGED_FIELD_VALUE
                            | EVENT_FORM_SUBMITTED)) {
                return USER_SELECT_SUGGESTION_USER_CHANGE_FORM_FORM_SUBMITTED;
            } else if (mState
                    == (EVENT_VIRTUAL_STRUCTURE_PROVIDED | EVENT_SUGGESTION_DISPLAYED
                            | EVENT_FORM_AUTOFILLED | EVENT_USER_CHANGED_FIELD_VALUE)) {
                return USER_SELECT_SUGGESTION_USER_CHANGE_FORM_NO_FORM_SUBMITTED;
            } else if (mState == (EVENT_VIRTUAL_STRUCTURE_PROVIDED | EVENT_SUGGESTION_DISPLAYED)) {
                return USER_NOT_SELECT_SUGGESTION_USER_NOT_CHANGE_FORM_NO_FORM_SUBMITTED;
            } else if (mState
                    == (EVENT_VIRTUAL_STRUCTURE_PROVIDED | EVENT_SUGGESTION_DISPLAYED
                            | EVENT_FORM_SUBMITTED)) {
                return USER_NOT_SELECT_SUGGESTION_USER_NOT_CHANGE_FORM_FORM_SUBMITTED;
            } else if (mState
                    == (EVENT_VIRTUAL_STRUCTURE_PROVIDED | EVENT_SUGGESTION_DISPLAYED
                            | EVENT_USER_CHANGED_FIELD_VALUE | EVENT_FORM_SUBMITTED)) {
                return USER_NOT_SELECT_SUGGESTION_USER_CHANGE_FORM_FORM_SUBMITTED;
            } else if (mState
                    == (EVENT_VIRTUAL_STRUCTURE_PROVIDED | EVENT_SUGGESTION_DISPLAYED
                            | EVENT_USER_CHANGED_FIELD_VALUE)) {
                return USER_NOT_SELECT_SUGGESTION_USER_CHANGE_FORM_NO_FORM_SUBMITTED;
            } else {
                return SESSION_UNKNOWN;
            }
        }

        private int mState;
        private Boolean mUserChangedAutofilledField;

        // Indicates whether the server prediction arrives.
        private boolean mServerPredictionAvailable;
    }

    /**
     * The class to record Autofill.WebView.ServerPrediction.AwGSuggestion, is only instantiated
     * when the Android platform AutofillServcie is AwG, This will give us more actual result in
     * A/B experiment while only AwG supports the server prediction.
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
                sample = mAutofilled ? AWG_HAS_SUGGESTION_AUTOFILLED
                                     : AWG_HAS_SUGGESTION_NO_AUTOFILL;
            }
            RecordHistogram.recordEnumeratedHistogram(
                    UMA_AUTOFILL_AWG_SUGGSTION_AVAILABILITY, sample, AWG_SUGGSTION_AVAILABLE_COUNT);
        }
    }

    private SessionRecorder mRecorder;
    private Boolean mAutofillDisabled;

    private final boolean mIsAwGCurrentAutofillService;
    private ServerPredictionRecorder mServerPredictionRecorder;

    public AutofillProviderUMA(Context context, boolean isAwGCurrentAutofillService) {
        RecordHistogram.recordBooleanHistogram(UMA_AUTOFILL_CREATED_BY_ACTIVITY_CONTEXT,
                ContextUtils.activityFromContext(context) != null);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            RecordHistogram.recordBooleanHistogram(
                    UMA_AUTOFILL_AWG_IS_CURRENT_SERVICE, isAwGCurrentAutofillService);
        }
        mIsAwGCurrentAutofillService = isAwGCurrentAutofillService;
    }

    public void onFormSubmitted(int submissionSource) {
        if (mRecorder != null) mRecorder.record(SessionRecorder.EVENT_FORM_SUBMITTED);
        recordSession();
        if (mServerPredictionRecorder != null) mServerPredictionRecorder.recordHistograms();
        // We record this no matter autofill service is disabled or not.
        RecordHistogram.recordEnumeratedHistogram(UMA_AUTOFILL_SUBMISSION_SOURCE,
                toUMASubmissionSource(submissionSource), SUBMISSION_SOURCE_HISTOGRAM_COUNT);
    }

    public void onSessionStarted(boolean autofillDisabled) {
        // Record autofill status once per instance and only if user triggers the autofill.
        if (mAutofillDisabled == null || mAutofillDisabled.booleanValue() != autofillDisabled) {
            RecordHistogram.recordBooleanHistogram(UMA_AUTOFILL_ENABLED, !autofillDisabled);
            mAutofillDisabled = Boolean.valueOf(autofillDisabled);
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

    public void onUserChangeFieldValue(boolean isPreviouslyAutofilled) {
        if (mRecorder == null) return;
        if (isPreviouslyAutofilled) {
            mRecorder.record(SessionRecorder.EVENT_USER_CHANGED_AUTOFILLED_FIELD);
        } else {
            mRecorder.record(SessionRecorder.EVENT_USER_CHANGED_FIELD_VALUE);
        }
    }

    /**
     * Invoked when the server query was done or has arrived when the autofill sension starts.
     *
     * @param formData the form of the current session, is null if the query failed.
     * @param afterSessionStarted true if the server type predication arrive after the session
     *         starts.
     */
    public void onServerTypeAvailable(FormData formData, boolean afterSessionStarted) {
        mRecorder.onServerTypeAvailable(formData, afterSessionStarted);
    }

    private void recordSession() {
        if (mAutofillDisabled != null && !mAutofillDisabled.booleanValue() && mRecorder != null) {
            mRecorder.recordHistogram();
        }
        mRecorder = null;
    }

    private int toUMASubmissionSource(int source) {
        switch (source) {
            case SubmissionSource.SAME_DOCUMENT_NAVIGATION:
                return SAME_DOCUMENT_NAVIGATION;
            case SubmissionSource.XHR_SUCCEEDED:
                return XHR_SUCCEEDED;
            case SubmissionSource.FRAME_DETACHED:
                return FRAME_DETACHED;
            case SubmissionSource.DOM_MUTATION_AFTER_XHR:
                return DOM_MUTATION_AFTER_XHR;
            case SubmissionSource.PROBABLY_FORM_SUBMITTED:
                return PROBABLY_FORM_SUBMITTED;
            case SubmissionSource.FORM_SUBMISSION:
                return FORM_SUBMISSION;
            default:
                return SUBMISSION_SOURCE_HISTOGRAM_COUNT;
        }
    }
}
