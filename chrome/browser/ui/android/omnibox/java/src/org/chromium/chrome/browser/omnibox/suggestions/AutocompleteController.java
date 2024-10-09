// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.omnibox.OmniboxMetrics;
import org.chromium.chrome.browser.omnibox.suggestions.action.OmniboxAnswerAction;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler.VoiceResult;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteResult;
import org.chromium.components.omnibox.AutocompleteResult.VerificationPoint;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

import java.util.HashSet;
import java.util.List;
import java.util.Optional;
import java.util.Set;

/**
 * Bridge to the native AutocompleteControllerAndroid.
 *
 * <p>The bridge is created and maintained by the AutocompleteControllerAndroid native class. The
 * Native class is created on request for supplied profiles and remains available until the Profile
 * gets destroyed, making this instance follow the same life cycle.
 *
 * <p>Instances of this class should not be acquired directly; instead, when a profile-specific
 * AutocompleteController is required, please acquire one using the AutocompleteControllerFactory.
 *
 * <p>When User Profile gets destroyed, native class gets destroyed as well, and during the
 * destruction calls the #notifyNativeDestroyed() method, which signals the Java
 * AutocompleteController is no longer valid, and removes it from the AutocompleteControllerFactory
 * cache.
 */
public class AutocompleteController {
    // Maximum number of voice suggestions to show.
    private static final int MAX_VOICE_SUGGESTION_COUNT = 3;

    private final @NonNull Set<OnSuggestionsReceivedListener> mListeners = new HashSet<>();
    private long mNativeController;
    private @NonNull Optional<AutocompleteResult> mAutocompleteResult = Optional.empty();

    /** Listener for receiving OmniboxSuggestions. */
    public interface OnSuggestionsReceivedListener {
        /**
         * Receive autocomplete matches for currently executing query.
         *
         * @param autocompleteResult The current set of autocomplete matches for previously supplied
         *     query.
         * @param isFinal Whether this result is transitory (false) or final (true). Final result
         *     always comes in last, even if the query is canceled.
         */
        void onSuggestionsReceived(@NonNull AutocompleteResult autocompleteResult, boolean isFinal);
    }

    /**
     * Acquire an instance of AutocompleteController associated with the supplied Profile.
     *
     * @return An existing (if one is available) or new (otherwise) instance of the
     *     AutocompleteController associated with the supplied profile.
     */
    @CalledByNative
    private AutocompleteController(long nativeController) {
        mNativeController = nativeController;
    }

    /**
     * @param listener The listener to be notified when new suggestions are available.
     */
    public void addOnSuggestionsReceivedListener(@NonNull OnSuggestionsReceivedListener listener) {
        mListeners.add(listener);
    }

    /**
     * @param listener A previously registered new suggestions listener to be removed.
     */
    public void removeOnSuggestionsReceivedListener(
            @NonNull OnSuggestionsReceivedListener listener) {
        mListeners.remove(listener);
    }

    /**
     * Starts querying for omnibox suggestions for a given text.
     *
     * @param url The URL of the current tab, used to suggest query refinements.
     * @param pageClassification The page classification of the current tab.
     * @param text The text to query autocomplete suggestions for.
     * @param cursorPosition The position of the cursor within the text. Set to -1 if the cursor is
     *     not focused on the text.
     * @param preventInlineAutocomplete Whether autocomplete suggestions should be prevented.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public void start(
            @NonNull GURL url,
            int pageClassification,
            @NonNull String text,
            int cursorPosition,
            boolean preventInlineAutocomplete) {
        if (mNativeController == 0) return;

        AutocompleteControllerJni.get()
                .start(
                        mNativeController,
                        text,
                        cursorPosition,
                        null,
                        url.getSpec(),
                        pageClassification,
                        preventInlineAutocomplete,
                        false,
                        false,
                        true);
    }

    /**
     * Issue a prefetch request for zero prefix suggestions. Prefetch is a fire-and-forget operation
     * that yields no results.
     *
     * @param url The URL of the current tab, used to suggest query refinements.
     * @param pageClassification The page classification of the current tab.
     */
    void startPrefetch(@NonNull GURL url, int pageClassification) {
        if (mNativeController == 0) return;
        AutocompleteControllerJni.get()
                .startPrefetch(mNativeController, url.getSpec(), pageClassification);
    }

    /**
     * Given some string |text| that the user wants to use for navigation, determines how it should
     * be interpreted. This is a fallback in case the user didn't select a visible suggestion (e.g.
     * the user pressed enter before omnibox suggestions had been shown).
     *
     * <p>Note: this updates the internal state of the autocomplete controller just as start() does.
     * Future calls that reference autocomplete results by index, e.g. onSuggestionSelected(),
     * should reference the returned suggestion by index 0.
     *
     * @param text The user's input text to classify (i.e. what they typed in the omnibox)
     * @return The AutocompleteMatch specifying where to navigate, the transition type, etc. May be
     *     null if the input is invalid.
     */
    public AutocompleteMatch classify(@NonNull String text) {
        if (mNativeController == 0) return null;
        return AutocompleteControllerJni.get().classify(mNativeController, text);
    }

    /**
     * Starts a query for suggestions before any input is available from the user.
     *
     * @param omniboxText The text displayed in the omnibox.
     * @param url The url of the currently loaded web page.
     * @param pageClassification The page classification of the current tab.
     * @param title The title of the currently loaded web page.
     * @param isOnFocusContext Whether the request is made on focus (as opposed to on a text
     *     change).
     */
    public void startZeroSuggest(
            @NonNull String omniboxText,
            @NonNull GURL url,
            int pageClassification,
            @NonNull String title,
            boolean isOnFocusContext) {
        if (mNativeController == 0) return;

        AutocompleteControllerJni.get()
                .onOmniboxFocused(
                        mNativeController,
                        omniboxText,
                        url.getSpec(),
                        pageClassification,
                        title,
                        isOnFocusContext);
    }

    /**
     * Stops generating autocomplete suggestions for the currently specified text from {@link
     * #start(Profile,String, String, boolean)}.
     *
     * @param clear Whether to clear the most recent autocomplete results. When true, the {@link
     *     #onSuggestionsReceived(AutocompleteResult, String)} will be called with an empty result
     *     set.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public void stop(boolean clear) {
        if (mNativeController == 0) return;
        AutocompleteControllerJni.get().stop(mNativeController, clear);
    }

    /**
     * Resets session for autocomplete controller. This happens every time we start typing new input
     * into the omnibox.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public void resetSession() {
        if (mNativeController == 0) return;
        AutocompleteControllerJni.get().resetSession(mNativeController);
    }

    private boolean hasValidNativeObjectRef(
            AutocompleteMatch match, @VerificationPoint int reason) {
        // Skip suggestions from cache.
        OmniboxMetrics.recordUsedSuggestionFromCache(match.getNativeObjectRef() == 0L);
        if (match.getNativeObjectRef() == 0L) return false;
        return mAutocompleteResult
                .map(res -> res.verifyCoherency(AutocompleteResult.NO_SUGGESTION_INDEX, reason))
                .orElse(false);
    }

    /**
     * Partially deletes an omnibox suggestion. This call should be used by compound suggestion
     * types (such as carousel) that host multiple components inside (eg. MostVisitedTiles).
     *
     * @param match the match to delete elements of
     * @param elementIndex the element within the match that needs to be deleted
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public void deleteMatchElement(AutocompleteMatch match, int elementIndex) {
        if (mNativeController == 0) return;
        if (!hasValidNativeObjectRef(match, VerificationPoint.DELETE_MATCH)) return;

        // Skip suggestions from cache.
        if (match.getNativeObjectRef() == 0L) return;
        AutocompleteControllerJni.get()
                .deleteMatchElement(mNativeController, match.getNativeObjectRef(), elementIndex);
    }

    /**
     * Deletes an omnibox suggestion, if possible.
     *
     * @param match the match to delete
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public void deleteMatch(AutocompleteMatch match) {
        if (mNativeController == 0) return;
        if (!hasValidNativeObjectRef(match, VerificationPoint.DELETE_MATCH)) return;

        // Skip suggestions from cache.
        if (match.getNativeObjectRef() == 0L) return;
        AutocompleteControllerJni.get().deleteMatch(mNativeController, match.getNativeObjectRef());
    }

    @CalledByNative
    @VisibleForTesting
    public void onSuggestionsReceived(
            @NonNull AutocompleteResult autocompleteResult, boolean isFinal) {
        mAutocompleteResult = Optional.of(autocompleteResult);

        // Notify callbacks of suggestions.
        for (OnSuggestionsReceivedListener listener : mListeners) {
            listener.onSuggestionsReceived(autocompleteResult, isFinal);
        }
    }

    @CalledByNative
    private void notifyNativeDestroyed() {
        mNativeController = 0;
    }

    /**
     * Called whenever a navigation happens from the omnibox to record metrics about the user's
     * interaction with the omnibox.
     *
     * @param match AutocompleteMatch that was selected by the user
     * @param suggestionLine the index of the line the match is presented on
     * @param disposition the window open disposition
     * @param currentPageUrl the URL of the current page
     * @param pageClassification the page classification of the current tab
     * @param elapsedTimeSinceModified the number of ms that passed between the user first modifying
     *     text in the omnibox and selecting a suggestion
     * @param completedLength the length of the default match's inline autocompletion if any
     * @param webContents the web contents for the tab where the selected suggestion will be shown
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public void onSuggestionSelected(
            AutocompleteMatch match,
            int suggestionLine,
            int disposition,
            @NonNull GURL currentPageUrl,
            int pageClassification,
            long elapsedTimeSinceModified,
            int completedLength,
            @Nullable WebContents webContents) {
        if (mNativeController == 0) return;
        if (!hasValidNativeObjectRef(match, VerificationPoint.SELECT_MATCH)) return;

        AutocompleteControllerJni.get()
                .onSuggestionSelected(
                        mNativeController,
                        match.getNativeObjectRef(),
                        suggestionLine,
                        disposition,
                        currentPageUrl.getSpec(),
                        pageClassification,
                        elapsedTimeSinceModified,
                        completedLength,
                        webContents);
    }

    /**
     * Create a native navigation observser on native side.
     *
     * @param navigationHandle The NavigationHandle for the current navigation.
     * @param match AutocompleteMatch that was selected by the user
     */
    void createNavigationObserver(NavigationHandle navigationHandle, AutocompleteMatch match) {
        if (mNativeController == 0) return;
        if (!hasValidNativeObjectRef(match, VerificationPoint.SELECT_MATCH)) return;

        AutocompleteControllerJni.get()
                .createNavigationObserver(
                        mNativeController,
                        navigationHandle.nativeNavigationHandlePtr(),
                        match.getNativeObjectRef());
    }

    /**
     * Called when the user touches down on a suggestion. Only called for search suggestions.
     *
     * @param match the match that received the touch
     * @param matchIndex the vertical position at which the match is located
     * @param webContents the web contents for the tab where suggestion could be used
     * @return whether or not a prefetch was started
     */
    public boolean onSuggestionTouchDown(
            AutocompleteMatch match, int matchIndex, @Nullable WebContents webContents) {
        if (mNativeController == 0) return false;
        if (!hasValidNativeObjectRef(match, VerificationPoint.ON_TOUCH_MATCH)) return false;

        return AutocompleteControllerJni.get()
                .onSuggestionTouchDown(
                        mNativeController, match.getNativeObjectRef(), matchIndex, webContents);
    }

    /**
     * Pass the voice provider a list representing the results of a voice recognition.
     *
     * @param results A list containing the results of a voice recognition.
     */
    void onVoiceResults(@Nullable List<VoiceResult> results) {
        if (mNativeController == 0) return;
        if (results == null || results.size() == 0) return;
        final int count = Math.min(results.size(), MAX_VOICE_SUGGESTION_COUNT);
        String[] voiceMatches = new String[count];
        float[] confidenceScores = new float[count];
        for (int i = 0; i < count; i++) {
            voiceMatches[i] = results.get(i).getMatch();
            confidenceScores[i] = results.get(i).getConfidence();
        }
        AutocompleteControllerJni.get()
                .setVoiceMatches(mNativeController, voiceMatches, confidenceScores);
    }

    /**
     * Updates searchbox stats parameters on the selected match that we will navigate to and
     * returns the updated URL.
     *
     * @param match the AutocompleteMatch object to get the updated destination URL for
     * @param elapsedTimeSinceInputChange the number of ms between the time the user started typing
     *     in the omnibox and the time the user has selected a suggestion
     */
    @Nullable
    GURL updateMatchDestinationUrlWithQueryFormulationTime(
            AutocompleteMatch match, long elapsedTimeSinceInputChange) {
        if (mNativeController == 0) return null;
        if (!hasValidNativeObjectRef(match, VerificationPoint.UPDATE_MATCH)) return null;

        return AutocompleteControllerJni.get()
                .updateMatchDestinationURLWithAdditionalSearchboxStats(
                        mNativeController, match.getNativeObjectRef(), elapsedTimeSinceInputChange);
    }

    /**
     * Returns the final url for navigating to the SRP for the given answer action. The returned URL
     * is augmented with the final searchbox stats.
     */
    @Nullable
    GURL getAnswerActionDestinationURL(
            AutocompleteMatch match,
            long elapsedTimeSinceInputChange,
            OmniboxAnswerAction answerAction) {
        if (mNativeController == 0) return null;
        assert hasValidNativeObjectRef(match, VerificationPoint.UPDATE_MATCH);
        if (!hasValidNativeObjectRef(match, VerificationPoint.UPDATE_MATCH)) return null;

        return AutocompleteControllerJni.get()
                .getAnswerActionDestinationURL(
                        mNativeController,
                        match.getNativeObjectRef(),
                        elapsedTimeSinceInputChange,
                        answerAction.getNativeInstance());
    }

    /**
     * Retrieves matching tab for suggestion at specific index.
     *
     * @param match the AutocompleteMatch to retrieve Tab info for
     * @return tab that hosts matching URL
     */
    @Nullable
    Tab getMatchingTabForSuggestion(AutocompleteMatch match) {
        if (mNativeController == 0) return null;
        if (!hasValidNativeObjectRef(match, VerificationPoint.GET_MATCHING_TAB)) return null;
        return AutocompleteControllerJni.get()
                .getMatchingTabForSuggestion(mNativeController, match.getNativeObjectRef());
    }

    /**
     * Pass the UI specific measurement information to Native code to aid Adaptive Suggestions.
     *
     * @param dropdownHeightWithKeyboardActive the height of visible part of the suggestions
     *     dropdown with software keyboard showing, expressed in pixels
     * @param suggestionHeight the nominal height of a suggestion, expressed in pixels
     */
    void onSuggestionDropdownHeightChanged(
            @Px int dropdownHeightWithKeyboardActive, @Px int suggestionHeight) {
        if (mNativeController == 0) return;
        AutocompleteControllerJni.get()
                .onSuggestionDropdownHeightChanged(
                        mNativeController, dropdownHeightWithKeyboardActive, suggestionHeight);
    }

    /**
     * Acquire an instance of AutocompleteController associated with the supplied Profile.
     *
     * @param profile The profile to get the AutocompleteController for.
     * @return An existing (if one is available) or new (otherwise) instance of the
     *     AutocompleteController associated with the supplied profile.
     */
    public static Optional<AutocompleteController> getForProfile(Profile profile) {
        return Optional.ofNullable(
                profile == null ? null : AutocompleteControllerJni.get().getForProfile(profile));
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    @NativeMethods
    public interface Natives {
        void start(
                long nativeAutocompleteControllerAndroid,
                String text,
                int cursorPosition,
                String desiredTld,
                String currentUrl,
                int pageClassification,
                boolean preventInlineAutocomplete,
                boolean preferKeyword,
                boolean allowExactKeywordMatch,
                boolean wantAsynchronousMatches);

        AutocompleteMatch classify(long nativeAutocompleteControllerAndroid, String text);

        void stop(long nativeAutocompleteControllerAndroid, boolean clearResults);

        void resetSession(long nativeAutocompleteControllerAndroid);

        void onSuggestionSelected(
                long nativeAutocompleteControllerAndroid,
                long nativeAutocompleteMatch,
                int matchIndex,
                int disposition,
                String currentPageUrl,
                int pageClassification,
                long elapsedTimeSinceModified,
                int completedLength,
                WebContents webContents);

        boolean onSuggestionTouchDown(
                long nativeAutocompleteControllerAndroid,
                long nativeAutocompleteMatch,
                int matchIndex,
                WebContents webContents);

        void onOmniboxFocused(
                long nativeAutocompleteControllerAndroid,
                String omniboxText,
                String currentUrl,
                int pageClassification,
                String currentTitle,
                boolean isOnFocusContext);

        void deleteMatchElement(
                long nativeAutocompleteControllerAndroid,
                long nativeAutocompleteMatch,
                int elementIndex);

        void deleteMatch(long nativeAutocompleteControllerAndroid, long nativeAutocompleteMatch);

        GURL updateMatchDestinationURLWithAdditionalSearchboxStats(
                long nativeAutocompleteControllerAndroid,
                long nativeAutocompleteMatch,
                long elapsedTimeSinceInputChange);

        GURL getAnswerActionDestinationURL(
                long nativeAutocompleteControllerAndroid,
                long nativeAutocompleteMatch,
                long elapsedTimeSinceInputChange,
                long nativeAnswerAction);

        Tab getMatchingTabForSuggestion(
                long nativeAutocompleteControllerAndroid, long nativeAutocompleteMatch);

        void setVoiceMatches(
                long nativeAutocompleteControllerAndroid,
                String[] matches,
                float[] confidenceScores);

        // Sends a zero suggest request to the server in order to pre-populate the result cache.
        void startPrefetch(
                long nativeAutocompleteControllerAndroid,
                String currentUrl,
                int pageClassification);

        // Create a navigation observser.
        void createNavigationObserver(
                long nativeAutocompleteControllerAndroid,
                long mNativeNavigationHandle,
                long nativeAutocompleteMatch);

        // Pass the information about the height of the visible Omnibox Dropdown area and
        // Suggestion Height expressed in Pixels.
        void onSuggestionDropdownHeightChanged(
                long nativeAutocompleteControllerAndroid,
                @Px int dropdownHeightWithKeyboardActive,
                @Px int suggestionHeight);

        /** Acquire an instance of AutocompleteController associated with the supplied profile. */
        AutocompleteController getForProfile(@JniType("Profile*") Profile profile);
    }
}
