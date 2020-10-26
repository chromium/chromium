// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

import android.content.Context;
import android.view.View;
import android.view.inputmethod.InputMethodManager;

import org.hamcrest.Matchers;
import org.junit.Assert;

import org.chromium.chrome.browser.omnibox.LocationBarLayout;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController.OnSuggestionsReceivedListener;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteCoordinatorTestUtils;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteResult;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestion;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionsDropdown;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.concurrent.Callable;

/**
 * Utility methods and classes for testing the Omnibox.
 */
public class OmniboxTestUtils {
    private OmniboxTestUtils() {}

    /**
     * Builder for the data structure that describes a set of omnibox results for a given
     * query.
     */
    public static class TestSuggestionResultsBuilder {
        private final List<SuggestionsResultBuilder> mSuggestionBuilders =
                new ArrayList<SuggestionsResultBuilder>();
        private String mTextShownFor;

        public TestSuggestionResultsBuilder addSuggestions(SuggestionsResultBuilder suggestions) {
            mSuggestionBuilders.add(suggestions);
            return this;
        }

        public TestSuggestionResultsBuilder setTextShownFor(String text) {
            mTextShownFor = text;
            return this;
        }

        private List<SuggestionsResult> buildSuggestionsList() {
            ArrayList<SuggestionsResult> suggestions = new ArrayList<SuggestionsResult>();
            for (int i = 0; i < mSuggestionBuilders.size(); i++) {
                suggestions.add(mSuggestionBuilders.get(i).build());
            }
            return suggestions;
        }
    }

    /**
     * Builder for {@link SuggestionsResult}.
     */
    public static class SuggestionsResultBuilder {
        private final List<OmniboxSuggestion> mSuggestions = new ArrayList<OmniboxSuggestion>();
        private String mAutocompleteText;

        public SuggestionsResultBuilder addSuggestion(OmniboxSuggestion suggestion) {
            mSuggestions.add(suggestion);
            return this;
        }

        public SuggestionsResultBuilder setAutocompleteText(String autocompleteText) {
            mAutocompleteText = autocompleteText;
            return this;
        }

        private SuggestionsResult build() {
            return new SuggestionsResult(mSuggestions, mAutocompleteText);
        }
    }

    /**
     * Data structure that contains the test data to be sent to
     * {@link OnSuggestionsReceivedListener#onSuggestionsReceived}.
     */
    public static class SuggestionsResult {
        private final List<OmniboxSuggestion> mSuggestions;
        private final String mAutocompleteText;

        public SuggestionsResult(List<OmniboxSuggestion> suggestions, String autocompleteText) {
            mSuggestions = suggestions;
            mAutocompleteText = autocompleteText;
        }
    }

    /**
     * Builds the necessary suggestion input for a TestAutocompleteController.
     */
    public static Map<String, List<SuggestionsResult>> buildSuggestionMap(
            TestSuggestionResultsBuilder... builders) {
        Map<String, List<SuggestionsResult>> suggestionMap =
                new HashMap<String, List<SuggestionsResult>>();
        for (TestSuggestionResultsBuilder builder : builders) {
            suggestionMap.put(builder.mTextShownFor, builder.buildSuggestionsList());
        }
        return suggestionMap;
    }

    /**
     * AutocompleteController instance that allows for easy testing.
     */
    public static class TestAutocompleteController extends AutocompleteController {
        private final View mView;
        private final Map<String, List<SuggestionsResult>> mSuggestions;
        private Runnable mSuggestionsDispatcher;
        private int mZeroSuggestCalledCount;
        private boolean mStartAutocompleteCalled;

        public TestAutocompleteController(View view, OnSuggestionsReceivedListener listener,
                Map<String, List<SuggestionsResult>> suggestions) {
            mView = view;
            mSuggestions = suggestions;
            setOnSuggestionsReceivedListener(listener);
        }

        @Override
        public void start(Profile profile, String url, int pageClassification, final String text,
                int cursorPosition, boolean preventInlineAutocomplete, String queryTileId,
                boolean isQueryStartedFromTiles) {
            mStartAutocompleteCalled = true;
            mSuggestionsDispatcher = new Runnable() {
                @Override
                public void run() {
                    List<SuggestionsResult> suggestions =
                            mSuggestions.get(text.toLowerCase(Locale.US));
                    if (suggestions == null) return;

                    for (int i = 0; i < suggestions.size(); i++) {
                        AutocompleteResult autocompleteResult =
                                new AutocompleteResult(suggestions.get(i).mSuggestions, null);
                        onSuggestionsReceived(
                                autocompleteResult, suggestions.get(i).mAutocompleteText, 0);
                    }
                }
            };
            mView.post(mSuggestionsDispatcher);
        }

        @Override
        public void groupSuggestionsBySearchVsURL(int firstIndex, int lastIndex) {}

        @Override
        public void startZeroSuggest(Profile profile, String omniboxText, String url,
                int pageClassification, String title) {
            mZeroSuggestCalledCount++;
        }

        public int numZeroSuggestRequests() {
            return mZeroSuggestCalledCount;
        }

        public boolean isStartAutocompleteCalled() {
            return mStartAutocompleteCalled;
        }

        @Override
        public void stop(boolean clear) {
            if (mSuggestionsDispatcher != null) mView.removeCallbacks(mSuggestionsDispatcher);
            mSuggestionsDispatcher = null;
        }

        @Override
        public void setProfile(Profile profile) {}
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
        public void groupSuggestionsBySearchVsURL(int firstIndex, int lastIndex) {}

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
                    AutocompleteCoordinatorTestUtils.getSuggestionsDropdown(
                            locationBar.getAutocompleteCoordinator());
            Criteria.checkThat(
                    "suggestion list is null", suggestionsDropdown, Matchers.notNullValue());
            Criteria.checkThat("suggestion list is not shown",
                    suggestionsDropdown.getViewGroup().isShown(), Matchers.is(true));
            Criteria.checkThat("suggestion list has no entries", suggestionsDropdown.getItemCount(),
                    Matchers.greaterThan(0));
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
                    AutocompleteCoordinatorTestUtils.getSuggestionsDropdown(
                            locationBar.getAutocompleteCoordinator());
            Criteria.checkThat(suggestionsDropdown, Matchers.notNullValue());
            Criteria.checkThat(suggestionsDropdown.getViewGroup().isShown(), Matchers.is(true));
            Criteria.checkThat(suggestionsDropdown.getItemCount(), Matchers.is(expectedCount));
        });
    }
}
