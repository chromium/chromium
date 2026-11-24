// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.ntp;

import static androidx.test.espresso.matcher.ViewMatchers.isAssignableFrom;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.chromium.base.test.transit.Condition.whether;

import android.view.View;

import androidx.annotation.IntDef;

import org.chromium.base.test.transit.Element;
import org.chromium.base.test.transit.OptionalViewElement;
import org.chromium.base.test.transit.Outcome;
import org.chromium.base.test.transit.SimpleConditions;
import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.TripBuilder;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.ntp.search.SearchBoxContainerView;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.url_constants.UrlConstantResolver;
import org.chromium.chrome.browser.url_constants.UrlOverrideUtils;
import org.chromium.chrome.test.transit.omnibox.OmniboxFacility;
import org.chromium.chrome.test.transit.page.CtaPageStation;
import org.chromium.chrome.test.transit.page.NativePageCondition;
import org.chromium.chrome.test.transit.page.WebPageStation;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Collections;
import java.util.List;
import java.util.Set;

/**
 * The New Tab Page screen, with an omnibox, most visited tiles, and the Feed instead of the
 * WebContents.
 */
public class RegularNewTabPageStation extends CtaPageStation {
    public ViewElement<View> searchBoxElement;
    public OptionalViewElement<View> ntpMicButtonElement;
    public ViewElement<UrlBar> urlBarElement;
    public @Nullable ViewElement<View> logoElement;
    public Element<NewTabPage> nativePageElement;

    /** The focused Omnibox, entered by default. Null unless the Station expects it focused. */
    public final @Nullable OmniboxFacility omnibox;

    /** What the Station checks about the search provider logo of the native NTP. */
    @IntDef({Logo.SHOWN, Logo.NOT_SHOWN, Logo.NOT_CHECKED})
    @Retention(RetentionPolicy.SOURCE)
    private @interface Logo {
        int SHOWN = 0;
        int NOT_SHOWN = 1;
        int NOT_CHECKED = 2;
    }

    private RegularNewTabPageStation(
            Config config, boolean nativeNtp, @Logo int logo, boolean omniboxFocused) {
        super(
                config.withIncognito(false)
                        .withExpectedUrlSubstring(
                                nativeNtp
                                        ? UrlConstantResolver.getOriginalNativeNtpUrl()
                                        : UrlConstantResolver.getOriginalNtpUrl()));

        declareElementFactory(
                mActivityElement,
                delayedElements -> {
                    if (mActivityElement.value().isTablet()) {
                        urlBarElement = delayedElements.declareView(URL_BAR);
                    }
                });

        omnibox =
                omniboxFocused
                        ? declareDefaultFacility(new OmniboxFacility(/* fakeSuggestions= */ null))
                        : null;

        if (nativeNtp) {
            if (logo == Logo.SHOWN) {
                logoElement = declareView(withId(R.id.search_provider_logo));
            } else if (logo == Logo.NOT_SHOWN) {
                declareNoView(withId(R.id.search_provider_logo));
            }
            // Scoped to the NTP's own fakebox class: R.id.search_box is also used by the Hub
            // toolbar (and by bookmarks and settings search boxes), and an unscoped matcher
            // here would match the Hub's search box while leaving the NTP for the Hub.
            // TODO(crbug.com/570644422): Replace isAssignableFrom(SearchBoxContainerView.class)
            // with SearchBoxContainerView.class once ViewSpec includes isAssignableFrom() in its
            // generated Matcher<View>.
            searchBoxElement =
                    declareView(
                            withId(R.id.search_box),
                            isAssignableFrom(SearchBoxContainerView.class));
            ntpMicButtonElement = declareOptionalView(withId(R.id.voice_search_button));

            nativePageElement =
                    declareEnterConditionAsElement(
                            new NativePageCondition<>(NewTabPage.class, loadedTabElement));
            declareEnterCondition(
                    SimpleConditions.uiThreadCondition(
                            "Regular NTP is loaded",
                            nativePageElement,
                            nativePage -> whether(nativePage.isLoadedForTests())));
        }
    }

    public static Builder<RegularNewTabPageStation> newBuilder() {
        return new Builder<>(
                c ->
                        new RegularNewTabPageStation(
                                c,
                                /* nativeNtp= */ !UrlOverrideUtils.isWebUiNtpOverrideEnabled(),
                                Logo.SHOWN,
                                /* omniboxFocused= */ false));
    }

    public static Builder<RegularNewTabPageStation> newNativeNtpBuilder() {
        return new Builder<>(
                c ->
                        new RegularNewTabPageStation(
                                c, /* nativeNtp= */ true, Logo.SHOWN, /* omniboxFocused= */ false));
    }

    public static Builder<RegularNewTabPageStation> newNativeNtpWithoutLogoBuilder() {
        return new Builder<>(
                c ->
                        new RegularNewTabPageStation(
                                c,
                                /* nativeNtp= */ true,
                                Logo.NOT_SHOWN,
                                /* omniboxFocused= */ false));
    }

    /**
     * Outcome: the NTP opened in a new tab from Chrome UI, e.g. the "New tab" menu item.
     *
     * <p>With {@link ChromeFeatureList#ANDROID_OMNIBOX_FOCUSED_NEW_TAB_PAGE}, the native NTP
     * focuses the Omnibox on load, so this enters {@link #omnibox}, skips the logo check, and skips
     * {@link #toolbar} on phones (which hide toolbar buttons while focused).
     */
    public static Outcome<RegularNewTabPageStation> openedInNewTabFrom(
            Station<ChromeTabbedActivity> origin) {
        boolean nativeNtp = !UrlOverrideUtils.isWebUiNtpOverrideEnabled();
        boolean omniboxFocused =
                nativeNtp
                        && ChromeFeatureList.isEnabled(
                                ChromeFeatureList.ANDROID_OMNIBOX_FOCUSED_NEW_TAB_PAGE);
        RegularNewTabPageStation ntp =
                new Builder<>(
                                c ->
                                        new RegularNewTabPageStation(
                                                c,
                                                nativeNtp,
                                                omniboxFocused ? Logo.NOT_CHECKED : Logo.SHOWN,
                                                omniboxFocused))
                        .initOpeningNewTab()
                        .build();
        Outcome<RegularNewTabPageStation> outcome = Outcome.enter(ntp);
        if (omniboxFocused && !origin.getActivity().isTablet()) {
            outcome = outcome.withoutDefaultFacility(ntp.toolbar);
        }
        return outcome;
    }

    /** Opens the app menu by pressing the toolbar "..." button */
    public RegularNewTabPageAppMenuFacility openAppMenu() {
        return toolbar.menuButtonElement
                .clickTo()
                .enterFacility(new RegularNewTabPageAppMenuFacility());
    }

    /**
     * Checks MVTs exist and returns an {@code MvtsFacility} with interactions with Most Visited
     * Tiles.
     *
     * @param siteSuggestions the expected SiteSuggestions to be displayed. Use fakes ones for
     *     testing.
     * @param separatorIndices the indices of separators between tiles.
     */
    public MvtsFacility focusOnMvts(
            List<SiteSuggestion> siteSuggestions, Set<Integer> separatorIndices) {
        // Assume MVTs are on the screen; if this assumption changes, make sure to scroll to them.
        return noopTo().enterFacility(new MvtsFacility(siteSuggestions, separatorIndices));
    }

    /** Same as {@link #focusOnMvts(List, Set)} expecting no separatorIndices. */
    public MvtsFacility focusOnMvts(List<SiteSuggestion> siteSuggestions) {
        return focusOnMvts(siteSuggestions, Collections.emptySet());
    }

    /**
     * Clicks the NTP mic button on the search box, expecting to navigate to a search results page.
     *
     * @param query the query to expect in the search URL
     * @return the {@link WebPageStation} representing the search results page
     */
    public WebPageStation clickNtpMicToSearchPage(String query) {
        return ntpMicButtonElement.clickTo().arriveAt(createSearchPageStation(query));
    }

    @Override
    protected TripBuilder clickUrlBarOrSearchBarTo() {
        // searchBoxElement is only declared for the native NTP. The WebUI NTP has no native
        // search box, so use the toolbar URL bar instead.
        if (searchBoxElement == null) {
            return urlBarElement.clickTo();
        }
        return searchBoxElement.clickTo();
    }
}
