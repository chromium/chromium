// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.hub;

import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParentIndex;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.instanceOf;

import static org.chromium.base.test.transit.ViewElement.unscopedOption;
import static org.chromium.base.test.transit.ViewSpec.viewSpec;

import android.view.KeyEvent;
import android.view.View;

import androidx.core.util.Pair;
import androidx.test.espresso.action.ViewActions;
import androidx.test.espresso.matcher.ViewMatchers;

import org.hamcrest.Matcher;

import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.base.test.transit.ViewSpec;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.omnibox.LocationBarLayout;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionView;
import org.chromium.chrome.browser.searchwidget.SearchActivity;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.OmniboxTestUtils.SuggestionsNotShownCondition;
import org.chromium.chrome.test.util.OmniboxTestUtils.SuggestionsShownCondition;
import org.chromium.chrome.test.util.OmniboxTestUtils.UrlBarHasFocusCondition;

import java.util.ArrayList;
import java.util.List;

/** The base station for Hub tab switcher stations. */
public class TabSwitcherSearchStation extends Station<SearchActivity> {
    private static final ViewSpec<View> SUGGESTIONS_LIST =
            viewSpec(withId(R.id.omnibox_results_container));

    private final boolean mIsIncognito;
    public ViewElement<LocationBarLayout> locationBarElement;
    public ViewElement<View> backButtonElement;
    public ViewElement<UrlBar> urlBarElement;

    public TabSwitcherSearchStation(boolean isIncognito) {
        super(SearchActivity.class);
        mIsIncognito = isIncognito;

        getActivityElement().expectActivityDestroyed();

        locationBarElement = declareView(LocationBarLayout.class, withId(R.id.search_location_bar));
        backButtonElement = declareView(withId(R.id.location_bar_status), unscopedOption());
        urlBarElement = declareView(UrlBar.class, withId(R.id.url_bar));
    }

    public boolean isIncognito() {
        return mIsIncognito;
    }

    public RegularTabSwitcherStation pressBackToRegularTabSwitcher(ChromeTabbedActivity activity) {
        assert !mIsIncognito;
        return backButtonElement
                .clickTo()
                .arriveAt(RegularTabSwitcherStation.from(activity.getTabModelSelector()));
    }

    public IncognitoTabSwitcherStation pressBackToIncognitoTabSwitcher(
            ChromeTabbedActivity activity) {
        assert mIsIncognito;
        return backButtonElement
                .clickTo()
                .arriveAt(IncognitoTabSwitcherStation.from(activity.getTabModelSelector()));
    }

    public void typeInOmnibox(String query) {
        noopTo().waitFor(new UrlBarHasFocusCondition(urlBarElement.value()));
        urlBarElement
                .typeTextTo(query)
                .withPossiblyAlreadyFulfilled()
                .waitFor(new SuggestionsShownCondition(locationBarElement.value()));
    }

    public void checkSuggestionsShown() {
        noopTo().waitFor(new SuggestionsShownCondition(locationBarElement.value()));
    }

    public void checkSuggestionsNotShown() {
        noopTo().waitFor(new SuggestionsNotShownCondition(locationBarElement.value()));
    }

    /** Expect a suggestion with the given |index|, |title| and |text| combination. */
    public SuggestionFacility findSuggestion(
            @Nullable Integer index, @Nullable String title, @Nullable String text) {
        SUGGESTIONS_LIST.printFromRoot();
        return noopTo().enterFacility(new SuggestionFacility(index, title, text));
    }

    /** Expect suggestions with all the given |texts|. */
    public void findSuggestionsByText(List<String> texts, String prefix) {
        SUGGESTIONS_LIST.printFromRoot();
        List<Facility<?>> allSuggestionFacilities = new ArrayList<>();
        for (String text : texts) {
            allSuggestionFacilities.add(
                    new SuggestionFacility(/* index= */ null, /* title= */ null, prefix + text));
        }
        noopTo().enterFacilities(allSuggestionFacilities.toArray(new Facility[0]));
    }

    /** Expect a suggestion with the given |index| and |text|. */
    public SectionHeaderFacility findSectionHeaderByIndexAndText(int index, String text) {
        SUGGESTIONS_LIST.printFromRoot();
        return noopTo().enterFacility(new SectionHeaderFacility(index, text));
    }

    /** A suggestion in the search results. */
    public class SuggestionFacility extends Facility<TabSwitcherSearchStation> {
        private final @Nullable String mText;
        public ViewElement<BaseSuggestionView> suggestionElement;

        public SuggestionFacility(
                @Nullable Integer index, @Nullable String title, @Nullable String text) {
            assert index != null || title != null || text != null;
            mText = text;

            List<Matcher<View>> matchers = new ArrayList<>();
            if (index != null) {
                matchers.add(withParentIndex(index));
            }
            if (title != null) {
                matchers.add(
                        hasDescendant(
                                allOf(
                                        withId(R.id.line_1),
                                        withText(title),
                                        withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE))));
            }
            if (text != null) {
                matchers.add(
                        hasDescendant(
                                allOf(
                                        withId(R.id.line_2),
                                        withText(text),
                                        withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE))));
            }
            matchers.add(instanceOf(BaseSuggestionView.class));
            matchers.add(isDescendantOfA(withId(R.id.omnibox_results_container)));

            Matcher<View>[] matchersArray = new Matcher[matchers.size()];
            matchers.toArray(matchersArray);

            suggestionElement = declareView(viewSpec(BaseSuggestionView.class, matchersArray));
        }

        public WebPageStation openPage() {
            return suggestionElement.clickTo().arriveAt(buildDestinationPageStation());
        }

        public Pair<RegularTabSwitcherStation, TabGroupDialogFacility> openTabGroup(
                ChromeTabbedActivity activity, List<Integer> tabIdsInGroup, String title) {
            RegularTabSwitcherStation tabSwitcher =
                    RegularTabSwitcherStation.from(activity.getTabModelSelector());
            TabGroupDialogFacility dialog =
                    new TabGroupDialogFacility<>(tabIdsInGroup, title, null);
            suggestionElement.clickTo().arriveAtAnd(tabSwitcher).enterFacility(dialog);
            return new Pair<>(tabSwitcher, dialog);
        }

        public WebPageStation openPagePressingEnter() {
            UrlBar urlBar = urlBarElement.value();
            noopTo().waitFor(new UrlBarHasFocusCondition(urlBar, /* active= */ true));
            return urlBarElement
                    .performViewActionTo(ViewActions.pressKey(KeyEvent.KEYCODE_ENTER))
                    .arriveAt(buildDestinationPageStation());
        }

        private WebPageStation buildDestinationPageStation() {
            return WebPageStation.newBuilder()
                    .withIncognito(mIsIncognito)
                    .withEntryPoint()
                    .withExpectedUrlSubstring(mText)
                    .build();
        }
    }

    /** A section header in the search results. */
    public static class SectionHeaderFacility extends Facility<TabSwitcherSearchStation> {
        public ViewElement<View> headerElement;

        public SectionHeaderFacility(int index, String text) {
            headerElement =
                    declareView(
                            viewSpec(
                                    withText(text),
                                    withParentIndex(index),
                                    isDescendantOfA(withId(R.id.omnibox_results_container))));
        }
    }
}
