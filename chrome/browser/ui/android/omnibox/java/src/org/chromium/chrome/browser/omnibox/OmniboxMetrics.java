// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.TimingMetric;
import org.chromium.chrome.browser.omnibox.suggestions.mostvisited.SuggestTileType;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.components.omnibox.EntityInfoProto.ActionInfo.ActionType;
import org.chromium.components.omnibox.action.ActionInSuggestUmaType;
import org.chromium.components.omnibox.action.OmniboxPedalId;
import org.chromium.components.omnibox.suggestions.OmniboxSuggestionUiType;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * This class collects a variety of different Omnibox related metrics.
 */
public class OmniboxMetrics {
    /**
     * Maximum number of suggest tile types we want to record.
     * Anything beyond this will be reported in the overflow bucket.
     */
    private static final int MAX_SUGGEST_TILE_TYPE_POSITION = 15;
    public static final int MAX_AUTOCOMPLETE_POSITION = 30;
    /**
     * Duration between the request for suggestions and the time the first (synchronous) reply is
     * converted to the UI model.
     */
    public static final String HISTOGRAM_SUGGESTIONS_REQUEST_TO_UI_MODEL_FIRST =
            "Android.Omnibox.SuggestionList.RequestToUiModel.First";
    /**
     * Duration between the request for suggestions and the time the last (asynchronous) reply is
     * converted to the UI model.
     */
    public static final String HISTOGRAM_SUGGESTIONS_REQUEST_TO_UI_MODEL_LAST =
            "Android.Omnibox.SuggestionList.RequestToUiModel.Last";

    @IntDef({RefineActionUsage.NOT_USED, RefineActionUsage.SEARCH_WITH_ZERO_PREFIX,
            RefineActionUsage.SEARCH_WITH_PREFIX, RefineActionUsage.SEARCH_WITH_BOTH,
            RefineActionUsage.COUNT})
    @Retention(RetentionPolicy.SOURCE)
    public @interface RefineActionUsage {
        int NOT_USED = 0; // User did not interact with Refine button.
        int SEARCH_WITH_ZERO_PREFIX = 1; // User interacted with Refine button in zero-prefix mode.
        int SEARCH_WITH_PREFIX = 2; // User interacted with Refine button in non-zero-prefix mode.
        int SEARCH_WITH_BOTH = 3; // User interacted with Refine button in both contexts.
        int COUNT = 4;
    }

    @IntDef({ActionInSuggestIntentResult.SUCCESS, ActionInSuggestIntentResult.BAD_URI_SYNTAX,
            ActionInSuggestIntentResult.ACTIVITY_NOT_FOUND, ActionInSuggestIntentResult.COUNT})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ActionInSuggestIntentResult {
        /// Intent started successfully.
        int SUCCESS = 0;
        /// Unable to deserialize intent: invalid syntax. Never recorded: http://b/279756377.
        int BAD_URI_SYNTAX = 1;
        /// Unable to start intent: no activity.
        int ACTIVITY_NOT_FOUND = 2;
        int COUNT = 3;
    }
    // TODO(crbug/1418077) export this from upstream.
    // See entity_info.proto, ActionType.
    private static final int ACTION_TYPE_COUNT = 20;

    /**
     * Record how long the Suggestion List needed to layout its content and children.
     */
    public static TimingMetric recordSuggestionListLayoutTime() {
        return TimingMetric.shortThreadTime("Android.Omnibox.SuggestionList.LayoutTime2");
    }

    /**
     * Record how long the Suggestion List needed to measure its content and children.
     */
    public static TimingMetric recordSuggestionListMeasureTime() {
        return TimingMetric.shortThreadTime("Android.Omnibox.SuggestionList.MeasureTime2");
    }

    /**
     * Record the amount of time needed to create a new suggestion view.
     * The type of view is intentionally ignored for this call.
     */
    public static TimingMetric recordSuggestionViewCreateTime() {
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
    public static void recordSuggestionViewReuseStats(int viewsCreated, int viewsReused) {
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
    public static void recordSuggestionsViewCreatedType(@OmniboxSuggestionUiType int type) {
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
    public static void recordSuggestionsViewReusedType(@OmniboxSuggestionUiType int type) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.Omnibox.SuggestionView.ReusedType", type, OmniboxSuggestionUiType.COUNT);
    }

    /**
     * Record whether the interaction with the Omnibox resulted with a navigation (true) or user
     * leaving the omnibox and suggestions list.
     *
     * @param focusResultedInNavigation Whether the user completed interaction with navigation.
     */
    public static void recordOmniboxFocusResultedInNavigation(boolean focusResultedInNavigation) {
        RecordHistogram.recordBooleanHistogram(
                "Omnibox.FocusResultedInNavigation", focusResultedInNavigation);
    }

    /**
     * Record the length of time between when omnibox gets focused and when a omnibox match is open.
     */
    public static void recordFocusToOpenTime(long focusToOpenTimeInMillis) {
        RecordHistogram.recordMediumTimesHistogram(
                "Omnibox.FocusToOpenTimeAnyPopupState3", focusToOpenTimeInMillis);
    }

    /**
     * Record whether the used suggestion originates from Cache or Autocomplete subsystem.
     *
     * @param isFromCache Whether the suggestion selected by the User comes from suggestion cache.
     */
    public static void recordUsedSuggestionFromCache(boolean isFromCache) {
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
    public static void recordRefineActionUsage(@RefineActionUsage int refineActionUsage) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.Omnibox.RefineActionUsage", refineActionUsage, RefineActionUsage.COUNT);
    }

    /**
     * Record the pedal shown when the user used the omnibox to go somewhere.
     *
     * @param pedalId the shown pedal's {@link OmniboxActionId}.
     */
    public static void recordPedalShown(@OmniboxPedalId int pedalId) {
        RecordHistogram.recordEnumeratedHistogram(
                "Omnibox.PedalShown", pedalId, OmniboxPedalId.TOTAL_COUNT);
    }

    /**
     * Record a pedal is used.
     *
    @param omniboxActionType the clicked pedal's {@link OmniboxActionId}.
     */
    public static void recordPedalUsed(@OmniboxPedalId int pedalId) {
        RecordHistogram.recordEnumeratedHistogram(
                "Omnibox.SuggestionUsed.Pedal", pedalId, OmniboxPedalId.TOTAL_COUNT);
    }

    /**
     * Record page class specific histogram reflecting whether the user scrolled the suggestions
     * list.
     *
     * @param pageClass Page classification.
     * @param wasScrolled Whether the suggestions list was scrolled.
     */
    public static void recordSuggestionsListScrolled(int pageClass, boolean wasScrolled) {
        RecordHistogram.recordBooleanHistogram(
                histogramName("Android.Omnibox.SuggestionsListScrolled", pageClass), wasScrolled);
    }

    /**
     * Record the kind of (MostVisitedURL/OrganicRepeatableSearch) Tile type the User opened.
     *
     * @param position The position of a tile in the carousel.
     * @param isSearchTile Whether tile being opened is a Search tile.
     */
    public static void recordSuggestTileTypeUsed(int position, boolean isSearchTile) {
        @SuggestTileType
        int tileType = isSearchTile ? SuggestTileType.SEARCH : SuggestTileType.URL;
        RecordHistogram.recordExactLinearHistogram(
                "Omnibox.SuggestTiles.SelectedTileIndex", position, MAX_SUGGEST_TILE_TYPE_POSITION);
        RecordHistogram.recordEnumeratedHistogram(
                "Omnibox.SuggestTiles.SelectedTileType", tileType, SuggestTileType.COUNT);
    }

    /**
     * Records relevant histogram(s) when a Journeys action is clicked in the omnibox. Not emitted
     * if the given position is <0.
     */
    public static void recordResumeJourneyClick(int position) {
        if (position < 0) return;
        RecordHistogram.recordExactLinearHistogram(
                "Omnibox.SuggestionUsed.ResumeJourney", position, MAX_AUTOCOMPLETE_POSITION);
    }

    /**
     * Records relevant histogram(s) when a Journeys action is shown in the omnibox. Not emitted if
     * the given position is <0.
     */
    public static void recordResumeJourneyShown(int position) {
        if (position < 0) return;
        RecordHistogram.recordEnumeratedHistogram(
                "Omnibox.ResumeJourneyShown", position, MAX_AUTOCOMPLETE_POSITION);
    }

    /**
     * Records the time elapsed between the two events:
     * - the suggestions were requested (as a result of User input), and
     * - the suggestions response was transformed to a UI model.
     *
     * @param isFirst specifies whether this is the first (synchronous), or the last (final)
     *         asynchronous, suggestions response received from the AutocompleteController
     * @param elapsedTimeMs specifies how much time has elapsed between the two events
     */
    public static void recordSuggestionRequestToModelTime(boolean isFirst, long elapsedTimeMs) {
        RecordHistogram.recordCustomTimesHistogram(isFirst
                        ? HISTOGRAM_SUGGESTIONS_REQUEST_TO_UI_MODEL_FIRST
                        : HISTOGRAM_SUGGESTIONS_REQUEST_TO_UI_MODEL_LAST,
                elapsedTimeMs, 1, 1000, 50);
    }

    /**
     * Record the outcome of ActionInSuggest chip interaction.
     *
     * @param intentResult the {@link #ActionInSuggestIntentResult} to record
     */
    public static final void recordActionInSuggestIntentResult(
            @ActionInSuggestIntentResult int intentResult) {
        RecordHistogram.recordEnumeratedHistogram("Android.Omnibox.ActionInSuggest.IntentResult",
                intentResult, ActionInSuggestIntentResult.COUNT);
    }

    /**
     * Record the Use of the Omnibox Action in Suggest.
     *
     * @param actionType the direct value of corresponding {@link
     *         EntityInfoProto.ActionInfo.ActionType}
     * @param isUsed whether the suggestion was clicked
     */
    public static void recordActionInSuggestUsed(int actionType) {
        RecordHistogram.recordEnumeratedHistogram("Omnibox.ActionInSuggest.Used",
                actionTypeToUmaType(actionType), ActionInSuggestUmaType.MAX_VALUE);
    }

    /**
     * Record the Presence of the Omnibox Action in Suggest.
     *
     * @param actionType the direct value of corresponding {@link
     *         EntityInfoProto.ActionInfo.ActionType}
     * @param isUsed whether the suggestion was visible
     */
    public static void recordActionInSuggestShown(int actionType) {
        RecordHistogram.recordEnumeratedHistogram("Omnibox.ActionInSuggest.Shown",
                actionTypeToUmaType(actionType), ActionInSuggestUmaType.MAX_VALUE);
    }

    /**
     * Translate ActionType to ActionInSuggestUmaType.
     *
     * @param type the type of Action in Suggest to translate.
     */
    private static @ActionInSuggestUmaType int actionTypeToUmaType(int type) {
        switch (type) {
            case ActionType.CALL_VALUE:
                return ActionInSuggestUmaType.CALL;
            case ActionType.DIRECTIONS_VALUE:
                return ActionInSuggestUmaType.DIRECTIONS;
            case ActionType.REVIEWS_VALUE:
                return ActionInSuggestUmaType.REVIEWS;
            default:
                return ActionInSuggestUmaType.UNKNOWN;
        }
    }

    /**
     * Translate the pageClass to a histogram suffix.
     *
     * @param histogram Histogram prefix.
     * @param pageClass Page classification to translate.
     * @return Metric name.
     */
    private static String histogramName(@NonNull String prefix, int pageClass) {
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
