// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

import android.content.Context;
import android.util.Pair;
import android.view.View;
import android.view.inputmethod.InputMethodManager;

import org.hamcrest.Matchers;
import org.junit.Assert;

import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.omnibox.LocationBarLayout;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController.OnSuggestionsReceivedListener;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteCoordinator;
import org.chromium.chrome.browser.omnibox.suggestions.DropdownItemViewInfo;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionsDropdown;
import org.chromium.chrome.browser.omnibox.suggestions.header.HeaderView;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.omnibox.AutocompleteResult;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

import java.util.HashMap;
import java.util.Locale;
import java.util.Map;
import java.util.concurrent.Callable;

/**
 * Utility methods and classes for testing the Omnibox.
 */
public class OmniboxTestUtils {
    private OmniboxTestUtils() {}

    /**
     * AutocompleteController instance that allows for easy testing.
     */
    public static class TestAutocompleteController extends AutocompleteController {
        private final Map<String, Pair<String, AutocompleteResult>> mAutocompleteResults;
        private final AutocompleteResult mEmptyResult;

        /**
         * Create new Autocomplete controller.
         * @param listener
         */
        public TestAutocompleteController(OnSuggestionsReceivedListener listener) {
            mAutocompleteResults = new HashMap<>();
            mEmptyResult = new AutocompleteResult(null, null);
            setOnSuggestionsReceivedListener(listener);
        }

        /**
         * Register new AutocompleteResult offered when the test user input matches the
         * forInputText.
         *
         * @param forInputText String to match against: user query.
         * @param autocompleteText Recommended default autocompletion.
         * @param autocompleteResult List of suggestions associated with the query.
         */
        public void addAutocompleteResult(String forInputText, String autocompleteText,
                AutocompleteResult autocompleteResult) {
            mAutocompleteResults.put(forInputText, new Pair(autocompleteText, autocompleteResult));
        }

        /**
         * Suppress any suggestion logging mechanisms so that artificially created suggestions
         * do not attempt to log selection.
         */
        @Override
        public void onSuggestionSelected(int selectedIndex, int disposition, int hashCode, int type,
                String currentPageUrl, int pageClassification, long elapsedTimeSinceModified,
                int completedLength, WebContents webContents) {}

        @Override
        public void start(Profile profile, String url, int pageClassification, final String text,
                int cursorPosition, boolean preventInlineAutocomplete, String queryTileId,
                boolean isQueryStartedFromTiles) {
            if (sendSuggestions(text)) return;
            super.start(profile, url, pageClassification, text, cursorPosition,
                    preventInlineAutocomplete, queryTileId, isQueryStartedFromTiles);
        }

        @Override
        public void startZeroSuggest(Profile profile, String omniboxText, String url,
                int pageClassification, String title) {
            if (sendSuggestions(omniboxText)) return;
            super.startZeroSuggest(profile, omniboxText, url, pageClassification, title);
        }

        private boolean sendSuggestions(String forText) {
            String autocompleteText = forText.toLowerCase(Locale.US);
            Pair<String, AutocompleteResult> autocompleteSet =
                    mAutocompleteResults.get(autocompleteText);
            if (autocompleteSet == null) return false;
            onSuggestionsReceived(autocompleteSet.second, autocompleteSet.first, 0);
            return true;
        }
    }

    /**
     * AutocompleteController instance that will trigger no suggestions.
     */
    public static class StubAutocompleteController extends AutocompleteController {
        public StubAutocompleteController() {
            super();
            setOnSuggestionsReceivedListener(new OnSuggestionsReceivedListener() {
                @Override
                public void onSuggestionsReceived(
                        AutocompleteResult autocompleteResult, String inlineAutocompleteText) {
                    Assert.fail("No autocomplete suggestions should be received");
                }
            });
        }

        @Override
        public void start(Profile profile, String url, int pageClassification, String text,
                int cursorPosition, boolean preventInlineAutocomplete, String queryTileId,
                boolean isQueryStartedFromTiles) {}

        @Override
        public void startZeroSuggest(Profile profile, String omniboxText, String url,
                int pageClassification, String title) {}

        @Override
        public void stop(boolean clear) {}

        @Override
        public void setProfile(Profile profile) {}
    }

    /**
     * Checks and verifies that the URL bar can request and release focus X times without issue.
     * @param urlBar The view to focus.
     * @param times The number of times focus should be requested and released.
     */
    public static void checkUrlBarRefocus(UrlBar urlBar, int times) {
        for (int i = 0; i < times; i++) {
            toggleUrlBarFocus(urlBar, true);
            waitForFocusAndKeyboardActive(urlBar, true);
            toggleUrlBarFocus(urlBar, false);
            waitForFocusAndKeyboardActive(urlBar, false);
        }
    }

    /**
     * Determines whether the UrlBar currently has focus.
     * @param urlBar The view to check focus on.
     * @return Whether the UrlBar has focus.
     */
    public static boolean doesUrlBarHaveFocus(final UrlBar urlBar) {
        return TestThreadUtils.runOnUiThreadBlockingNoException(new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return urlBar.hasFocus();
            }
        });
    }

    private static boolean isKeyboardActiveForView(final View view) {
        return TestThreadUtils.runOnUiThreadBlockingNoException(new Callable<Boolean>() {
            @Override
            public Boolean call() {
                InputMethodManager imm = (InputMethodManager) view.getContext().getSystemService(
                        Context.INPUT_METHOD_SERVICE);
                return imm.isActive(view);
            }
        });
    }

    /**
     * Toggles the focus state for the passed in UrlBar.
     * @param urlBar The UrlBar whose focus is being changed.
     * @param gainFocus Whether focus should be requested or cleared.
     */
    public static void toggleUrlBarFocus(final UrlBar urlBar, boolean gainFocus) {
        if (gainFocus) {
            // During early startup (before completion of its first onDraw), the UrlBar
            // is not focusable. Tests have to wait for that to happen before trying to focus it.
            CriteriaHelper.pollUiThread(() -> {
                Criteria.checkThat("UrlBar not shown.", urlBar.isShown(), Matchers.is(true));
                Criteria.checkThat(
                        "UrlBar not focusable.", urlBar.isFocusable(), Matchers.is(true));
            });

            TouchCommon.singleClickView(urlBar);
        } else {
            TestThreadUtils.runOnUiThreadBlocking(() -> { urlBar.clearFocus(); });
        }
    }

    /**
     * Waits for the UrlBar to have the expected focus state.
     *
     * @param urlBar The UrlBar whose focus is being inspected.
     * @param active Whether the UrlBar is expected to have focus or not.
     */
    public static void waitForFocusAndKeyboardActive(final UrlBar urlBar, final boolean active) {
        CriteriaHelper.pollInstrumentationThread(() -> {
            Criteria.checkThat("URL Bar did not have expected focus", doesUrlBarHaveFocus(urlBar),
                    Matchers.is(active));
            Criteria.checkThat("Keyboard did not reach expected state",
                    isKeyboardActiveForView(urlBar), Matchers.is(active));
        });
    }

    /**
     * Waits for a non-empty list of omnibox suggestions is shown.
     *
     * @param locationBar The LocationBar who owns the suggestions.
     */
    public static void waitForOmniboxSuggestions(final LocationBarLayout locationBar) {
        waitForOmniboxSuggestions(locationBar, CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL);
    }

    /**
     * Waits for a non-empty list of omnibox suggestions is shown.
     *
     * @param locationBar The LocationBar who owns the suggestions.
     * @param maxPollTimeMs The maximum time to wait for the suggestions to be visible.
     */
    public static void waitForOmniboxSuggestions(
            final LocationBarLayout locationBar, long maxPollTimeMs) {
        CriteriaHelper.pollUiThread(() -> {
            OmniboxSuggestionsDropdown suggestionsDropdown =
                    locationBar.getAutocompleteCoordinator().getSuggestionsDropdownForTest();
            Criteria.checkThat(
                    "suggestion list is null", suggestionsDropdown, Matchers.notNullValue());
            Criteria.checkThat("suggestion list is not shown",
                    suggestionsDropdown.getViewGroup().isShown(), Matchers.is(true));
            Criteria.checkThat("suggestion list has no entries",
                    suggestionsDropdown.getDropdownItemViewCountForTest(), Matchers.greaterThan(0));
        }, maxPollTimeMs, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    /**
     * Waits for a suggestion list to be shown with a specified number of entries.
     * @param locationBar The LocationBar who owns the suggestions.
     * @param expectedCount The number of suggestions expected to be shown.
     */
    public static void waitForOmniboxSuggestions(
            final LocationBarLayout locationBar, final int expectedCount) {
        CriteriaHelper.pollUiThread(() -> {
            OmniboxSuggestionsDropdown suggestionsDropdown =
                    locationBar.getAutocompleteCoordinator().getSuggestionsDropdownForTest();
            Criteria.checkThat(suggestionsDropdown, Matchers.notNullValue());
            Criteria.checkThat(suggestionsDropdown.getViewGroup().isShown(), Matchers.is(true));
            Criteria.checkThat(suggestionsDropdown.getDropdownItemViewCountForTest(),
                    Matchers.is(expectedCount));
        });
    }

    /**
     * @return The index of the first suggestion which is |type|.
     */
    public static int getIndexForFirstSuggestionOfType(
            LocationBarLayout locationBar, @OmniboxSuggestionUiType int type) {
        ModelList currentModels =
                locationBar.getAutocompleteCoordinator().getSuggestionModelListForTest();
        for (int i = 0; i < currentModels.size(); i++) {
            DropdownItemViewInfo info = (DropdownItemViewInfo) currentModels.get(i);
            if (info.type == type) return i;
        }
        return -1;
    }

    /**
     * Retrieve the Suggestion View for specific suggestion index.
     * Traverses the Suggestions list and skips over the Headers.
     *
     * @param <T> The type of the expected view. Inferred from call.
     * @param locationBar LocationBarLayout instance.
     * @param indexOfSuggestionView The index of the suggestion view (not including the headers).
     * @return The View corresponding to suggestion with specific index.
     */
    public static <T extends View> T getSuggestionViewAtPosition(
            LocationBarLayout locationBar, final int indexOfSuggestionView) {
        final AutocompleteCoordinator coordinator = locationBar.getAutocompleteCoordinator();
        final OmniboxSuggestionsDropdown dropdown = coordinator.getSuggestionsDropdownForTest();

        return TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            final int numViews = dropdown.getDropdownItemViewCountForTest();
            int nonHeaderViewIndex = 0;

            for (int childIndex = 0; childIndex < numViews; childIndex++) {
                View view = dropdown.getDropdownItemViewForTest(childIndex);
                if (view instanceof HeaderView) continue;

                if (nonHeaderViewIndex == indexOfSuggestionView) return (T) view;
                nonHeaderViewIndex++;
            }

            return null;
        });
    }
}
