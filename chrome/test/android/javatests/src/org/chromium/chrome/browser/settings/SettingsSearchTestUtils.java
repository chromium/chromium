// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.replaceText;
import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.isRoot;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;

import static org.chromium.chrome.browser.toolbar.top.ButtonHighlightMatcher.withHighlight;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.view.View;

import androidx.annotation.StringRes;

import org.hamcrest.Matcher;

import org.chromium.chrome.R;
import org.chromium.ui.test.util.ViewUtils;

/** Utility methods and custom matchers for Settings UI tests. */
public class SettingsSearchTestUtils {

    private SettingsSearchTestUtils() {}

    /**
     * Types a search query in the Settings search box.
     *
     * @param query The search query string.
     */
    public static void typeSearchQuery(String query) {
        onView(withId(R.id.search_box)).perform(click());
        onView(withId(R.id.search_query)).perform(replaceText(query));
    }

    /**
     * Clicks a search result row containing a text view with the given string resource in Settings
     * search.
     *
     * @param textResId The string resource ID for the descendant text view.
     */
    public static void clickSearchResult(@StringRes int textResId) {
        clickSearchResult(withText(textResId));
    }

    /**
     * Clicks a search result row matching the given descendant view matcher in Settings search.
     *
     * @param childMatcher Matcher for a descendant view within the search result row.
     */
    public static void clickSearchResult(Matcher<View> childMatcher) {
        // onViewWaiting for debounce and Search results to appear.
        onViewWaiting(allOf(withParent(withId(R.id.recycler_view)), hasDescendant(childMatcher)))
                .perform(click());
    }

    /**
     * Returns a matcher verifying that a preference row containing a text view with the given
     * string resource is highlighted.
     *
     * @param textResId The string resource ID for the descendant text view.
     */
    public static Matcher<View> highlighted(@StringRes int textResId) {
        return highlighted(withText(textResId));
    }

    /**
     * Returns a matcher verifying that a preference row containing {@code childMatcher} is
     * highlighted.
     *
     * @param childMatcher Matcher for a descendant view within the preference row.
     */
    public static Matcher<View> highlighted(Matcher<View> childMatcher) {
        return allOf(hasDescendant(childMatcher), withHighlight(true));
    }

    /** Asserts that no search results are found in Settings search. */
    public static void assertNoSearchResultsFound() {
        // Uses {@code ViewUtils.isEventuallyVisible} rather than {@code isDisplayed()} because the
        // keyboard can cover the empty state text on smaller devices, resulting in less than 51% of
        // the view being displayed.
        onView(isRoot())
                .check(
                        ViewUtils.isEventuallyVisible(
                                withText(R.string.search_in_settings_no_match)));
    }
}
