// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.TimingMetric;
import org.chromium.chrome.browser.omnibox.action.OmniboxPedalType;
import org.chromium.chrome.browser.omnibox.suggestions.mostvisited.SuggestTileType;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * This class collects a variety of different Suggestions related metrics.
 */
public class SuggestionsMetrics {
    /**
     * Maximum number of suggest tile types we want to record.
     * Anything beyond this will be reported in the overflow bucket.
     */
    private static final int MAX_SUGGEST_TILE_TYPE_POSITION = 15;
    public static final int MAX_AUTOCOMPLETE_POSITION = 30;

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

    /**
     * Record how long the Suggestion List needed to layout its content and children.
     */
    static final TimingMetric recordSuggestionListLayoutTime() {
        return TimingMetric.shortThreadTime("Android.Omnibox.SuggestionList.LayoutTime2");
    }

    /**
     * Record how long the Suggestion List needed to measure its content and children.
     */
    static final TimingMetric recordSuggestionListMeasureTime() {
        return TimingMetric.shortThreadTime("Android.Omnibox.SuggestionList.MeasureTime2");
    }

    /**
     * Record the amount of time needed to create a new suggestion view.
     * The type of view is intentionally ignored for this call.
     */
    static final TimingMetric recordSuggestionViewCreateTime() {
        return TimingMetric.shortThreadTime("Android.Omnibox.SuggestionView.CreateTime2");
    }

    /**
     * Record whether suggestion view was successfully reused.
     *
     * @param viewsCreated Number of views created during the input session. This should not be
     *         higher than the sum of all limits in HistogramRecordingRecycledViewPool.
     * @param viewsReused Ratio of views re-used to total views bound. Effectively captures the
     *         efficiency of view recycling.
     */
    static final void recordSuggestionViewReuseStats(int viewsCreated, int viewsReused) {
        RecordHistogram.recordCount100Histogram(
                "Android.Omnibox.SuggestionView.SessionViewsCreated", viewsCreated);
        RecordHistogram.recordCount100Histogram(
                "Android.Omnibox.SuggestionView.SessionViewsReused", viewsReused);
    }

    /**
     * Record the type of the suggestion view that had to be constructed.
     * Recorded view type could not be retrieved from the Recycled View Pool and had to
     * be re-created.
     * Relevant for Omnibox recycler view improvements.
     *
     * @param type The type of view that needed to be recreated.
     */
    static final void recordSuggestionsViewCreatedType(@OmniboxSuggestionUiType int type) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.Omnibox.SuggestionView.CreatedType", type, OmniboxSuggestionUiType.COUNT);
    }

    /**
     * Record the type of the suggestion view that was re-used.
     * Recorded view type was retrieved from the Recycled View Pool.
     * Relevant for Omnibox recycler view improvements.
     *
     * @param type The type of view that was reused from pool.
     */
    static final void recordSuggestionsViewReusedType(@OmniboxSuggestionUiType int type) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.Omnibox.SuggestionView.ReusedType", type, OmniboxSuggestionUiType.COUNT);
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
     * Record the kind of (MostVisitedURL/OrganicRepeatableSearch) Tile type the User opened.
     *
     * @param position The position of a tile in the carousel.
     * @param isSearchTile Whether tile being opened is a Search tile.
     */
    public static final void recordSuggestTileTypeUsed(int position, boolean isSearchTile) {
        @SuggestTileType
        int tileType = isSearchTile ? SuggestTileType.SEARCH : SuggestTileType.URL;
        RecordHistogram.recordExactLinearHistogram(
                "Omnibox.SuggestTiles.SelectedTileIndex", position, MAX_SUGGEST_TILE_TYPE_POSITION);
        RecordHistogram.recordEnumeratedHistogram(
                "Omnibox.SuggestTiles.SelectedTileType", tileType, SuggestTileType.COUNT);
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
                // TODO(crbug.com/1314765): This assert fails persistently on tablets.
                // assert false : "Unknown page classification: " + pageClass;
                break;
        }

        return prefix + "." + suffix;
    }
}
