// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.tab_search;

import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParentIndex;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.instanceOf;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.test.transit.ViewElement.unscopedOption;
import static org.chromium.base.test.transit.ViewSpec.viewSpec;

import android.view.KeyEvent;
import android.view.View;
import android.widget.ImageView;

import androidx.annotation.Nullable;
import androidx.test.espresso.action.ViewActions;
import androidx.test.espresso.matcher.ViewMatchers;

import org.hamcrest.Matcher;
import org.hamcrest.Matchers;

import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.LocationBarLayout;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionView;
import org.chromium.chrome.test.transit.page.BasePageStation;
import org.chromium.chrome.test.transit.page.CtaPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.OmniboxTestUtils.SuggestionsNotShownCondition;
import org.chromium.chrome.test.util.OmniboxTestUtils.SuggestionsShownCondition;
import org.chromium.chrome.test.util.OmniboxTestUtils.UrlBarHasFocusCondition;

import java.util.ArrayList;
import java.util.List;

/**
 * Public Transit Facility representing the Tab Search Overlay popup.
 *
 * <p>The overlay displays a scrim behind a floating card containing an omnibox search bar, a close
 * button, and a suggestions dropdown list.
 *
 * @param <HostStationT> the type of host {@link CtaPageStation} where this overlay is displayed.
 */
public class TabSearchOverlayFacility<HostStationT extends CtaPageStation>
        extends Facility<HostStationT> {

    public ViewElement<View> scrimElement;
    public ViewElement<View> panelElement;
    public ViewElement<ImageView> closeButtonElement;
    public ViewElement<LocationBarLayout> locationBarElement;
    public ViewElement<UrlBar> urlBarElement;

    public TabSearchOverlayFacility() {
        super("TabSearchOverlayFacility");
        scrimElement = declareView(withId(R.id.tab_search_overlay_scrim));
        panelElement = declareView(withId(R.id.tab_search_overlay_panel));
        closeButtonElement = declareView(ImageView.class, withId(R.id.tab_search_close_button));
        locationBarElement = declareView(LocationBarLayout.class, withId(R.id.search_location_bar));
        urlBarElement =
                declareView(
                        UrlBar.class,
                        withId(R.id.url_bar),
                        isDescendantOfA(withId(R.id.tab_search_overlay_panel)));
    }

    /** Types the given query into the overlay's search box and waits for suggestions to appear. */
    public void typeInSearchBox(String query) {
        noopTo().waitFor(new UrlBarHasFocusCondition(urlBarElement.value()));
        urlBarElement
                .typeTextTo(query)
                .withPossiblyAlreadyFulfilled()
                .waitFor(new SuggestionsShownCondition(locationBarElement.value()));
    }

    /** Checks that suggestions are currently shown. */
    public void checkSuggestionsShown() {
        noopTo().waitFor(new SuggestionsShownCondition(locationBarElement.value()));
    }

    /** Checks that suggestions are not currently shown. */
    public void checkSuggestionsNotShown() {
        noopTo().waitFor(new SuggestionsNotShownCondition(locationBarElement.value()));
    }

    /** Finds a tab suggestion (open tab or history) in the suggestions list. */
    public TabSuggestionFacility findTabSuggestion(
            @Nullable Integer index, @Nullable String title, @Nullable String urlSubstring) {
        return noopTo().enterFacility(new TabSuggestionFacility(index, title, urlSubstring));
    }

    /** Finds a tab group suggestion in the suggestions list. */
    public TabGroupSuggestionFacility findTabGroupSuggestion(
            @Nullable Integer index, @Nullable String groupTitle) {
        return noopTo().enterFacility(
                        new TabGroupSuggestionFacility(
                                index, groupTitle, /* groupSubtitle= */ null));
    }

    /** Dismisses the overlay by clicking on the scrim background. */
    public void dismissViaScrim() {
        scrimElement.clickTo().exitFacility();
    }

    /** Dismisses the overlay by clicking on the close button. */
    public void dismissViaCloseButton() {
        closeButtonElement.clickTo().exitFacility();
    }

    /** Dismisses the overlay by pressing the back button / key. */
    public void dismissViaBack() {
        pressBackTo().exitFacility();
    }

    /** Base class for a suggestion in the Tab Search results list. */
    public abstract class BaseSuggestionFacility extends Facility<HostStationT> {
        protected final @Nullable String mTitle;
        protected final @Nullable String mText;
        public ViewElement<BaseSuggestionView> suggestionElement;

        public BaseSuggestionFacility(
                String facilityName,
                @Nullable Integer index,
                @Nullable String title,
                @Nullable String text) {
            super(facilityName);
            assertTrue(
                    "At least one criteria (index, title, or text) must be provided",
                    index != null || title != null || text != null);
            mTitle = title;
            mText = text;

            List<Matcher<View>> matchers = new ArrayList<>();
            if (index != null) {
                matchers.add(withParentIndex(index));
            }
            if (title != null) {
                Matcher<String> titleMatcher = Matchers.containsString(title);

                matchers.add(
                        hasDescendant(
                                allOf(
                                        withId(R.id.line_1),
                                        withText(titleMatcher),
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
            matchers.add(
                    isDescendantOfA(
                            allOf(
                                    withId(R.id.search_activity_suggestions_container),
                                    withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE))));

            @SuppressWarnings("unchecked")
            Matcher<View>[] matchersArray = new Matcher[matchers.size()];
            matchers.toArray(matchersArray);

            suggestionElement =
                    declareView(
                            viewSpec(BaseSuggestionView.class, matchersArray), unscopedOption());
        }
    }

    /** A suggestion representing an individual tab (open tab or history navigation). */
    public class TabSuggestionFacility extends BaseSuggestionFacility {
        public TabSuggestionFacility(
                @Nullable Integer index, @Nullable String title, @Nullable String urlSubstring) {
            super("TabSuggestionFacility", index, title, urlSubstring);
        }

        /** Clicks the suggestion to select the tab and arrive at a WebPageStation. */
        public WebPageStation clickToSelectTab() {
            return suggestionElement.clickTo().arriveAt(buildSelectedTabStation());
        }

        /** Clicks the suggestion to open in a new tab and arrive at a WebPageStation. */
        public WebPageStation clickToOpenNewTab() {
            return suggestionElement.clickTo().arriveAt(buildNewTabStation());
        }

        /**
         * Presses Enter while the suggestion is focused to select the tab and arrive at a
         * WebPageStation.
         */
        public WebPageStation pressEnterToSelectTab() {
            UrlBar urlBar = urlBarElement.value();
            noopTo().waitFor(new UrlBarHasFocusCondition(urlBar));
            return urlBarElement
                    .performViewActionTo(ViewActions.pressKey(KeyEvent.KEYCODE_ENTER))
                    .arriveAt(buildSelectedTabStation());
        }

        private WebPageStation buildSelectedTabStation() {
            BasePageStation.Builder<WebPageStation> builder =
                    WebPageStation.newBuilder()
                            .withIncognito(mHostStation.isIncognito())
                            .initSelectingExistingTab();
            if (mTitle != null) {
                builder.withExpectedTitle(mTitle);
            }
            if (mText != null) {
                builder.withExpectedUrlSubstring(mText);
            }
            return builder.build();
        }

        private WebPageStation buildNewTabStation() {
            BasePageStation.Builder<WebPageStation> builder =
                    WebPageStation.newBuilder()
                            .withIncognito(mHostStation.isIncognito())
                            .initOpeningNewTab();
            if (mTitle != null) {
                builder.withExpectedTitle(mTitle);
            }
            if (mText != null) {
                builder.withExpectedUrlSubstring(mText);
            }
            return builder.build();
        }
    }

    /** A suggestion representing a tab group in the Tab Search results list. */
    public class TabGroupSuggestionFacility extends BaseSuggestionFacility {
        public TabGroupSuggestionFacility(
                @Nullable Integer index,
                @Nullable String groupTitle,
                @Nullable String groupSubtitle) {
            super("TabGroupSuggestionFacility", index, groupTitle, groupSubtitle);
        }

        /**
         * Clicks the tab group suggestion to switch to the group and arrive at a WebPageStation.
         *
         * <p>Note: Does not match against the tab group's title because the destination tab's web
         * page title does not necessarily equal the group title.
         */
        public WebPageStation clickToSelectTabGroup() {
            return suggestionElement.clickTo().arriveAt(buildTabGroupStation());
        }

        /**
         * Presses Enter while the suggestion is focused to switch to the group and arrive at a
         * WebPageStation.
         */
        public WebPageStation pressEnterToSelectTabGroup() {
            UrlBar urlBar = urlBarElement.value();
            noopTo().waitFor(new UrlBarHasFocusCondition(urlBar));
            return urlBarElement
                    .performViewActionTo(ViewActions.pressKey(KeyEvent.KEYCODE_ENTER))
                    .arriveAt(buildTabGroupStation());
        }

        private WebPageStation buildTabGroupStation() {
            return WebPageStation.newBuilder()
                    .withIncognito(mHostStation.isIncognito())
                    .initSelectingExistingTab()
                    .build();
        }
    }
}
