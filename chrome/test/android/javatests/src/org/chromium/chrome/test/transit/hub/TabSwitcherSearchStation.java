// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.hub;

import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.withClassName;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParentIndex;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.containsString;
import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.Matchers.allOf;

import static org.chromium.base.test.transit.ViewSpec.viewSpec;

import android.view.KeyEvent;
import android.view.View;

import org.hamcrest.Matcher;

import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.base.test.transit.ViewSpec;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionView;
import org.chromium.chrome.browser.searchwidget.SearchActivity;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.ui.KeyboardUtils;
import org.chromium.ui.test.util.ViewUtils;

/** The base station for Hub tab switcher stations. */
public class TabSwitcherSearchStation extends Station<SearchActivity> {
    public static final ViewSpec SUGGESTIONS_LIST =
            viewSpec(withId(R.id.omnibox_results_container));

    private final boolean mIsIncognito;
    private OmniboxTestUtils mOmniboxTestUtils;
    public ViewElement<UrlBar> urlBarElement;

    public TabSwitcherSearchStation(boolean isIncognito) {
        super(SearchActivity.class);
        mIsIncognito = isIncognito;
        urlBarElement = declareView(viewSpec(UrlBar.class, withId(R.id.url_bar)));
    }

    public boolean isIncognito() {
        return mIsIncognito;
    }

    public void focusAndDropSoftKeyboard() {
        maybeInitSearchUtils();
        mOmniboxTestUtils.requestFocus();
        mOmniboxTestUtils.checkFocus(true);
        urlBarElement.check((v, nve) -> KeyboardUtils.hideAndroidSoftKeyboard(v));
    }

    public void typeInOmnibox(String query) {
        maybeInitSearchUtils();
        mOmniboxTestUtils.typeText(query, /* execute= */ false);
        mOmniboxTestUtils.checkSuggestionsShown(true);
    }

    public void checkSuggestionsShown(boolean shown) {
        maybeInitSearchUtils();
        mOmniboxTestUtils.checkSuggestionsShown(shown);
    }

    /** Returns a matcher for the matching index/title combo. */
    public Matcher<View> getSuggestionAtIndexWithTitleText(int index, String title) {
        return SUGGESTIONS_LIST
                .descendant(
                        allOf(
                                withParentIndex(index),
                                withClassName(equalTo(BaseSuggestionView.class.getName())),
                                hasDescendant(
                                        allOf(
                                                withId(R.id.line_1),
                                                withText(containsString(title))))))
                .getViewMatcher();
    }

    /** Waits for the suggestion with the index/title combo. */
    public void waitForSuggestionAtIndexWithTitleText(int index, String title) {
        SUGGESTIONS_LIST.printFromRoot();
        ViewUtils.waitForVisibleView(getSuggestionAtIndexWithTitleText(index, title));
    }

    public void waitForSectionAtIndexWithText(int index, String text) {
        SUGGESTIONS_LIST.printFromRoot();
        ViewUtils.waitForVisibleView(
                SUGGESTIONS_LIST
                        .descendant(allOf(withParentIndex(index), withText(containsString(text))))
                        .getViewMatcher());
    }

    public void pressEnter() {
        maybeInitSearchUtils();
        mOmniboxTestUtils.sendKey(KeyEvent.KEYCODE_ENTER);
    }

    private void maybeInitSearchUtils() {
        if (mOmniboxTestUtils == null) {
            mOmniboxTestUtils = new OmniboxTestUtils(getActivity());
        }
    }
}
