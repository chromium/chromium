// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.os.Debug;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.omnibox.action.OmniboxPedalType;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.concurrent.TimeUnit;

/**
 * This class collects a variety of different Suggestions related metrics.
 */
public class SuggestionsMetrics {
    // The expected range for measure and layout durations.
    // If the combination of (N*Create)+Measure+Layout, where N is a maximum number of suggestions
    // exceeds:
    // - 16'000us (16ms), the suggestion list will take more than a single frame on modern devices,
    // - 33'000us (33ms), the suggestion list will take more than a single frame on all devices.
    // We expect the reported values to be within 2milliseconds range.
    // The upper range here is deliberately smaller than the limits mentioned above; any stage
    // taking more time than the limit chosen here is an indication of a problem.
    private static final long MIN_HISTOGRAM_DURATION_US = 100;
    private static final long MAX_HISTOGRAM_DURATION_US = TimeUnit.MILLISECONDS.toMicros(10);
    private static final int NUM_DURATION_BUCKETS = 100;

    @IntDef({RefineActionUsage.NOT_USED, RefineActionUsage.SEARCH_WITH_ZERO_PREFIX,
            RefineActionUsage.SEARCH_WITH_PREFIX, RefineActionUsage.SEARCH_WITH_BOTH,
            RefineActionUsage.COUNT})
    @Retention(RetentionPolicy.SOURCE)
    @interface RefineActionUsage {
        int NOT_USED = 0; // User did not interact with Refine button.
        int SEARCH_WITH_ZERO_PREFIX = 1; // User interacted with Refine button in zero-prefix mode.
        int SEARCH_WITH_PREFIX = 2; // User interacted with Refine button in non-zero-prefix mode.
        int SEARCH_WITH_BOTH = 3; // User interacted with Refine button in both contexts.
        int COUNT = 4;
    }

    /** Class for reporting timing metrics using try-with-resources block. */
    public static class TimingMetric implements AutoCloseable {
        private final String mMetricName;
        private final long mStartTime;

        /**
         * Creates an instance and starts the timer.
         * @param metricName The name of the metric to be reported on close.
         */
        public TimingMetric(String metricName) {
            mMetricName = metricName;
            mStartTime = Debug.threadCpuTimeNanos();
        }

        @Override
        public void close() {
            final long endTime = Debug.threadCpuTimeNanos();
            final long duration = getDurationInMicroseconds(mStartTime, endTime);
            if (duration < 0) return;
            RecordHistogram.recordCustomTimesHistogram(mMetricName, duration,
                    MIN_HISTOGRAM_DURATION_US, MAX_HISTOGRAM_DURATION_US, NUM_DURATION_BUCKETS);
        }
    }

    /**
     * Measure duration between two timestamps in microseconds.
     *
     * @param startTimeNs Operation start timestamp (nanoseconds).
     * @param endTimeNs Operation end time (nanoseconds).
     * @return Duration in hundreds of microseconds or -1, if timestamps were invalid.
     */
    private static final long getDurationInMicroseconds(long startTimeNs, long endTimeNs) {
        if (startTimeNs == -1 || endTimeNs == -1) return -1;
        return TimeUnit.NANOSECONDS.toMicros(endTimeNs - startTimeNs);
    }

    /**
     * Record how long the Suggestion List needed to layout its content and children.
     */
    static final SuggestionsMetrics.TimingMetric recordSuggestionListLayoutTime() {
        return new TimingMetric("Android.Omnibox.SuggestionList.LayoutTime");
    }

    /**
     * Record how long the Suggestion List needed to measure its content and children.
     */
    static final SuggestionsMetrics.TimingMetric recordSuggestionListMeasureTime() {
        return new TimingMetric("Android.Omnibox.SuggestionList.MeasureTime");
    }

    /**
     * Record the amount of time needed to create a new suggestion view.
     * The type of view is intentionally ignored for this call.
     */
    static final SuggestionsMetrics.TimingMetric recordSuggestionViewCreateTime() {
        return new TimingMetric("Android.Omnibox.SuggestionView.CreateTime");
    }

    /**
     * Record whether suggestion view was successfully reused.
     *
     * @param reused Whether suggestion view was reused.
     */
    static final void recordSuggestionViewReused(boolean reused) {
        RecordHistogram.recordBooleanHistogram("Android.Omnibox.SuggestionView.Reused", reused);
    }

    /**
     * Record whether the interaction with the Omnibox resulted with a navigation (true) or user
     * leaving the omnibox and suggestions list.
     *
     * @param focusResultedInNavigation Whether the user completed interaction with navigation.
     */
    static final void recordOmniboxFocusResultedInNavigation(boolean focusResultedInNavigation) {
        RecordHistogram.recordBooleanHistogram(
                "Omnibox.FocusResultedInNavigation", focusResultedInNavigation);
    }

    /**
     * Record the length of time between when omnibox gets focused and when a omnibox match is open.
     */
    static final void recordFocusToOpenTime(long focusToOpenTimeInMillis) {
        RecordHistogram.recordMediumTimesHistogram(
                "Omnibox.FocusToOpenTimeAnyPopupState3", focusToOpenTimeInMillis);
    }

    /**
     * Record whether the used suggestion originates from Cache or Autocomplete subsystem.
     *
     * @param isFromCache Whether the suggestion selected by the User comes from suggestion cache.
     */
    static final void recordUsedSuggestionFromCache(boolean isFromCache) {
        RecordHistogram.recordBooleanHistogram(
                "Android.Omnibox.UsedSuggestionFromCache", isFromCache);
    }

    /**
     * Record the Refine action button usage.
     * Unlike the MobileOmniobxRefineSuggestion UserAction, this is recorded only once at the end of
     * an Omnibox interaction, and includes the cases where the user has not interacted with the
     * Refine button at all.
     *
     * @param refineActionUsage Whether - and how Refine action button was used.
     */
    static final void recordRefineActionUsage(@RefineActionUsage int refineActionUsage) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.Omnibox.RefineActionUsage", refineActionUsage, RefineActionUsage.COUNT);
    }

    /**
     * Record the pedal shown when the user used the omnibox to go somewhere.
     *
     * @param type the shown pedal's {@link OmniboxActionType}.
     */
    public static final void recordPedalShown(@OmniboxPedalType int type) {
        RecordHistogram.recordEnumeratedHistogram(
                "Omnibox.PedalShown", type, OmniboxPedalType.TOTAL_COUNT);
    }

    /**
     * Record a pedal is used.
     *
    @param omniboxActionType the clicked pedal's {@link OmniboxActionType}.
     */
    public static final void recordPedalUsed(@OmniboxPedalType int type) {
        RecordHistogram.recordEnumeratedHistogram(
                "Omnibox.SuggestionUsed.Pedal", type, OmniboxPedalType.TOTAL_COUNT);
    }

    /**
     * Record page class specific histogram reflecting whether the user scrolled the suggestions
     * list.
     *
     * @param pageClass Page classification.
     * @param wasScrolled Whether the suggestions list was scrolled.
     */
    static final void recordSuggestionsListScrolled(int pageClass, boolean wasScrolled) {
        RecordHistogram.recordBooleanHistogram(
                histogramName("Android.Omnibox.SuggestionsListScrolled", pageClass), wasScrolled);
    }

    /**
     * Translate the pageClass to a histogram suffix.
     *
     * @param histogram Histogram prefix.
     * @param pageClass Page classification to translate.
     * @return Metric name.
     */
    private static final String histogramName(@NonNull String prefix, int pageClass) {
        String suffix = "Other";

        switch (pageClass) {
            case PageClassification.NTP_VALUE:
            case PageClassification.INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS_VALUE:
            case PageClassification.INSTANT_NTP_WITH_FAKEBOX_AS_STARTING_FOCUS_VALUE:
            case PageClassification.START_SURFACE_NEW_TAB_VALUE:
            case PageClassification.START_SURFACE_HOMEPAGE_VALUE:
                suffix = "NTP";
                break;

            case PageClassification.SEARCH_RESULT_PAGE_DOING_SEARCH_TERM_REPLACEMENT_VALUE:
            case PageClassification.SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT_VALUE:
                suffix = "SRP";
                break;

            case PageClassification.ANDROID_SEARCH_WIDGET_VALUE:
            case PageClassification.ANDROID_SHORTCUTS_WIDGET_VALUE:
                suffix = "Widget";
                break;

            case PageClassification.BLANK_VALUE:
            case PageClassification.HOME_PAGE_VALUE:
            case PageClassification.OTHER_VALUE:
                // use default value for websites.
                break;

            default:
                // Report an error, but fall back to a default value.
                // Use this to detect missing new cases.
                assert false : "Unknown page classification: " + pageClass;
                break;
        }

        return prefix + "." + suffix;
    }
}
