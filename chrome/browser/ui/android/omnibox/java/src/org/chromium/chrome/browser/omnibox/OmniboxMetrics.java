// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.TimingMetric;
import org.chromium.chrome.browser.omnibox.suggestions.mostvisited.SuggestTileType;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.suggestions.OmniboxSuggestionUiType;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Optional;

/** This class collects a variety of different Omnibox related metrics. */
public class OmniboxMetrics {
    /**
     * Maximum number of suggest tile types we want to record. Anything beyond this will be reported
     * in the overflow bucket.
     */
    private static final int MAX_SUGGEST_TILE_TYPE_POSITION = 15;

    public static final int MAX_AUTOCOMPLETE_POSITION = 30;

    /**
     * Duration between the request for suggestions and the time the first (synchronous) reply is
     * converted to the UI model.
     */
    @VisibleForTesting
    public static final String HISTOGRAM_SUGGESTIONS_REQUEST_TO_UI_MODEL_FIRST =
            "Android.Omnibox.SuggestionList.RequestToUiModel.First";

    /**
     * Duration between the request for suggestions and the time the last (asynchronous) reply is
     * converted to the UI model.
     */
    @VisibleForTesting
    public static final String HISTOGRAM_SUGGESTIONS_REQUEST_TO_UI_MODEL_LAST =
            "Android.Omnibox.SuggestionList.RequestToUiModel.Last";

    /** Android.Omnibox.OmniboxAction.* histograms */
    @VisibleForTesting
    public static final String HISTOGRAM_OMNIBOX_ACTION_USED = "Android.Omnibox.OmniboxAction.Used";

    @VisibleForTesting
    public static final String HISTOGRAM_OMNIBOX_ACTION_VALID =
            "Android.Omnibox.OmniboxAction.Valid";

    public static final String HISTOGRAM_FOCUS_TO_IME_ANIMATION_START =
            "Android.Omnibox.SuggestionList.FocusToImeAnimationStart";

    /**
     * The amount of time it takes to process a touch down event. A touch down event can send a
     * signal to native to start a prefetch for the suggestion.
     */
    public static final String HISTOGRAM_SEARCH_PREFETCH_TOUCH_DOWN_PROCESS_TIME =
            "Android.Omnibox.SearchPrefetch.TouchDownProcessTime.NavigationPrefetch";

    /** The number of prefetches started in an omnibox session via the touch down trigger. */
    public static final String HISTOGRAM_SEARCH_PREFETCH_NUM_PREFETCHES_STARTED_IN_OMNIBOX_SESSION =
            "Android.Omnibox.SearchPrefetch.NumPrefetchesStartedInOmniboxSession.NavigationPrefetch";

    /** The result of prefetches started by touch down events within an omnibox session */
    public static final String HISTOGRAM_SEARCH_PREFETCH_TOUCH_DOWN_PREFETCH_RESULT =
            "Android.Omnibox.SearchPrefetch.TouchDownPrefetchResult.NavigationPrefetch";

    @IntDef({
        RefineActionUsage.NOT_USED,
        RefineActionUsage.SEARCH_WITH_ZERO_PREFIX,
        RefineActionUsage.SEARCH_WITH_PREFIX,
        RefineActionUsage.SEARCH_WITH_BOTH,
        RefineActionUsage.COUNT
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface RefineActionUsage {
        int NOT_USED = 0; // User did not interact with Refine button.
        int SEARCH_WITH_ZERO_PREFIX = 1; // User interacted with Refine button in zero-prefix mode.
        int SEARCH_WITH_PREFIX = 2; // User interacted with Refine button in non-zero-prefix mode.
        int SEARCH_WITH_BOTH = 3; // User interacted with Refine button in both contexts.
        int COUNT = 4;
    }

    @IntDef({
        ActionInSuggestIntentResult.SUCCESS,
        ActionInSuggestIntentResult.BAD_URI_SYNTAX,
        ActionInSuggestIntentResult.ACTIVITY_NOT_FOUND,
        ActionInSuggestIntentResult.COUNT
    })
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

    @IntDef({
        PrefetchResult.HIT,
        PrefetchResult.MISS,
        PrefetchResult.NO_PREFETCH,
        PrefetchResult.COUNT
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface PrefetchResult {
        // The last prefetch started matches the suggestion navigated to.
        int HIT = 0;
        // The last prefetch started does NOT match the suggesiton navigated to.
        int MISS = 1;
        // No prefetches were stated in the omnibox session.
        int NO_PREFETCH = 2;
        int COUNT = 3;
    }

    /**
     * Record how long the Suggestion List needed to layout its content and children in thread time.
     */
    public static TimingMetric recordSuggestionListLayoutTime() {
        return TimingMetric.shortThreadTime("Android.Omnibox.SuggestionList.LayoutTime2");
    }

    /**
     * Record how long the Suggestion List needed to layout its content and children in wall time.
     */
    public static TimingMetric recordSuggestionListLayoutWallTime() {
        return TimingMetric.shortUptime("Android.Omnibox.SuggestionList.LayoutTime3");
    }

    /**
     * Record how long the Suggestion List needed to measure its content and children in thread
     * time.
     */
    public static TimingMetric recordSuggestionListMeasureTime() {
        return TimingMetric.shortThreadTime("Android.Omnibox.SuggestionList.MeasureTime2");
    }

    /**
     * Record how long the Suggestion List needed to measure its content and children in wall time.
     */
    public static TimingMetric recordSuggestionListMeasureWallTime() {
        return TimingMetric.shortThreadTime("Android.Omnibox.SuggestionList.MeasureTime3");
    }

    /**
     * Record the amount of time needed to create a new suggestion view. The type of view is
     * intentionally ignored for this call.
     */
    public static TimingMetric recordSuggestionViewCreateTime() {
        return TimingMetric.shortThreadTime("Android.Omnibox.SuggestionView.CreateTime2");
    }

    /** Record the amount of wall time needed to create a new suggestion view. */
    public static TimingMetric recordSuggestionViewCreateWallTime() {
        return TimingMetric.shortThreadTime("Android.Omnibox.SuggestionView.CreateTime3");
    }

    /**
     * Record whether suggestion view was successfully reused.
     *
     * @param viewsCreated Number of views created during the input session. This should not be
     *     higher than the sum of all limits in HistogramRecordingRecycledViewPool.
     * @param viewsReused Ratio of views re-used to total views bound. Effectively captures the
     *     efficiency of view recycling.
     */
    public static void recordSuggestionViewReuseStats(int viewsCreated, int viewsReused) {
        RecordHistogram.recordCount100Histogram(
                "Android.Omnibox.SuggestionView.SessionViewsCreated", viewsCreated);
        RecordHistogram.recordCount100Histogram(
                "Android.Omnibox.SuggestionView.SessionViewsReused", viewsReused);
    }

    /**
     * Record the type of the suggestion view that had to be constructed. Recorded view type could
     * not be retrieved from the Recycled View Pool and had to be re-created. Relevant for Omnibox
     * recycler view improvements.
     *
     * @param type The type of view that needed to be recreated.
     */
    public static void recordSuggestionsViewCreatedType(@OmniboxSuggestionUiType int type) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.Omnibox.SuggestionView.CreatedType", type, OmniboxSuggestionUiType.COUNT);
    }

    /**
     * Record the type of the suggestion view that was re-used. Recorded view type was retrieved
     * from the Recycled View Pool. Relevant for Omnibox recycler view improvements.
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
     * Record the Refine action button usage. Unlike the MobileOmniobxRefineSuggestion UserAction,
     * this is recorded only once at the end of an Omnibox interaction, and includes the cases where
     * the user has not interacted with the Refine button at all.
     *
     * @param refineActionUsage Whether - and how Refine action button was used.
     */
    public static void recordRefineActionUsage(@RefineActionUsage int refineActionUsage) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.Omnibox.RefineActionUsage", refineActionUsage, RefineActionUsage.COUNT);
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
        @SuggestTileType int tileType = isSearchTile ? SuggestTileType.SEARCH : SuggestTileType.URL;
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
     *
     * <ul>
     *   <li>the suggestions were requested (as a result of User input), and
     *   <li>the suggestions response was transformed to a UI model.
     * </ul>
     *
     * @param isFirst specifies whether this is the first (synchronous), or the last (final)
     *     asynchronous, suggestions response received from the AutocompleteController
     * @param elapsedTimeMs specifies how much time has elapsed between the two events
     */
    public static void recordSuggestionRequestToModelTime(boolean isFirst, long elapsedTimeMs) {
        RecordHistogram.recordCustomTimesHistogram(
                isFirst
                        ? HISTOGRAM_SUGGESTIONS_REQUEST_TO_UI_MODEL_FIRST
                        : HISTOGRAM_SUGGESTIONS_REQUEST_TO_UI_MODEL_LAST,
                elapsedTimeMs,
                1,
                1000,
                50);
    }

    /**
     * Record the outcome of ActionInSuggest chip interaction.
     *
     * @param intentResult the {@link #ActionInSuggestIntentResult} to record
     */
    public static final void recordActionInSuggestIntentResult(
            @ActionInSuggestIntentResult int intentResult) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.Omnibox.ActionInSuggest.IntentResult",
                intentResult,
                ActionInSuggestIntentResult.COUNT);
    }

    /**
     * Record whether an OmniboxAction was still valid when the user finished the interaction with
     * the Omnibox.
     *
     * <p>Recorded once for every action currently offered to the user at the time when the user
     * completed the interaction with the Omnibox.
     */
    public static void recordOmniboxActionIsValid(boolean isValid) {
        RecordHistogram.recordBooleanHistogram(HISTOGRAM_OMNIBOX_ACTION_VALID, isValid);
    }

    /**
     * Record whether any OmniboxAction was used by the User to complete interaction with the
     * Omnibox.
     *
     * <p>Recorded once for *every interaction with the Omnibox* where OmniboxActions were shown to
     * the user at the final stage of interaction.
     */
    public static void recordOmniboxActionIsUsed(boolean wasUsed) {
        RecordHistogram.recordBooleanHistogram(HISTOGRAM_OMNIBOX_ACTION_USED, wasUsed);
    }

    /**
     * Record the amount of time it takes to process a touch down event. This can include sending
     * event to native and then starting a prefetch for the suggestion.
     */
    public static TimingMetric recordTouchDownProcessTime() {
        return TimingMetric.shortThreadTime(HISTOGRAM_SEARCH_PREFETCH_TOUCH_DOWN_PROCESS_TIME);
    }

    /**
     * Records the number of prefetches started by touch down events in an omnibox session.
     *
     * @param numPrefetchesStarted the number of prefetches started wihin the omnibox session.
     */
    public static void recordNumPrefetchesStartedInOmniboxSession(int numPrefetchesStarted) {
        RecordHistogram.recordCount100Histogram(
                HISTOGRAM_SEARCH_PREFETCH_NUM_PREFETCHES_STARTED_IN_OMNIBOX_SESSION,
                numPrefetchesStarted);
    }

    /**
     * Records the result of prefetches started by touch down events within an omnibox session.
     *
     * @param navSuggestion the suggestion that was navigated to.
     * @param prefetchSuggestion the most recent suggestion that a prefetch was started for. This
     *     value is null if no prefetches have been started in the current omnibox session.
     */
    public static void recordTouchDownPrefetchResult(
            @NonNull AutocompleteMatch navSuggestion,
            @NonNull Optional<AutocompleteMatch> prefetchSuggestion) {
        @PrefetchResult
        int result =
                prefetchSuggestion
                        .map(
                                match ->
                                        navSuggestion.getNativeObjectRef() != 0
                                                        && navSuggestion.getNativeObjectRef()
                                                                == match.getNativeObjectRef()
                                                ? PrefetchResult.HIT
                                                : PrefetchResult.MISS)
                        .orElse(PrefetchResult.NO_PREFETCH);

        RecordHistogram.recordEnumeratedHistogram(
                HISTOGRAM_SEARCH_PREFETCH_TOUCH_DOWN_PREFETCH_RESULT, result, PrefetchResult.COUNT);
    }

    /**
     * Records the wall time elapsed between focusing the omnibox and the onPrepare event of the IME
     * WindowInsets animation.
     */
    public static TimingMetric recordTimeFromFocusToImeAnimation() {
        return TimingMetric.shortUptime(HISTOGRAM_FOCUS_TO_IME_ANIMATION_START);
    }

    /**
     * Translate the pageClass to a histogram suffix.
     *
     * @param prefix Histogram prefix.
     * @param pageClass Page classification to translate.
     * @return Metric name.
     */
    private static String histogramName(@NonNull String prefix, int pageClass) {
        String suffix = "Other";

        switch (pageClass) {
            case PageClassification.INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS_VALUE:
            case PageClassification.NTP_REALBOX_VALUE:
            case PageClassification.NTP_VALUE:
            case PageClassification.NTP_ZPS_PREFETCH_VALUE:
            case PageClassification.SEARCH_BUTTON_AS_STARTING_FOCUS_VALUE:
            case PageClassification.START_SURFACE_HOMEPAGE_VALUE:
            case PageClassification.START_SURFACE_NEW_TAB_VALUE:
                suffix = "NTP";
                break;

            case PageClassification.LENS_SIDE_PANEL_SEARCHBOX_VALUE:
            case PageClassification.SEARCH_RESULT_PAGE_DOING_SEARCH_TERM_REPLACEMENT_VALUE:
            case PageClassification.SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT_VALUE:
            case PageClassification.SEARCH_RESULT_PAGE_ON_CCT_VALUE:
            case PageClassification.SEARCH_SIDE_PANEL_SEARCHBOX_VALUE:
            case PageClassification.SRP_ZPS_PREFETCH_VALUE:
                suffix = "SRP";
                break;

            case PageClassification.ANDROID_SEARCH_WIDGET_VALUE:
            case PageClassification.ANDROID_SHORTCUTS_WIDGET_VALUE:
                suffix = "Widget";
                break;

            case PageClassification.ANDROID_HUB_VALUE:
                suffix = "HUB";
                break;

            case PageClassification.BLANK_VALUE:
            case PageClassification.CONTEXTUAL_SEARCHBOX_VALUE:
            case PageClassification.HOME_PAGE_VALUE:
            case PageClassification.JOURNEYS_VALUE:
            case PageClassification.OTHER_ON_CCT_VALUE:
            case PageClassification.OTHER_VALUE:
            case PageClassification.OTHER_ZPS_PREFETCH_VALUE:
                // use default value for websites.
                break;

            case PageClassification.OBSOLETE_INSTANT_NTP_VALUE:
            case PageClassification.OBSOLETE_INSTANT_NTP_WITH_FAKEBOX_AS_STARTING_FOCUS_VALUE:
                assert false
                        : "Obsolete page classification. Please use the OMNIBOX variant instead.";
                break;

            default:
                // May trigger if nev PageClassifications were added to
                // third_party/metrics_proto/omnibox_event.proto file,
                // but have not been reflected here. If that's the case, file a bug for the
                // author of the new PageClassification.
                // Last supported value: OTHER_ON_CCT.
                assert false
                        : "b/40221519: Invalid page classification: "
                                + pageClass
                                + ". Please re-open bug, and attach captured stack trace.";
                break;
        }

        return prefix + "." + suffix;
    }
}
