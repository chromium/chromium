// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.ntp;

import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.chromium.base.test.transit.ViewSpec.viewSpec;

import android.util.Pair;

import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.ViewSpec;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.test.transit.SoftKeyboardFacility;
import org.chromium.chrome.test.transit.omnibox.FakeOmniboxSuggestions;
import org.chromium.chrome.test.transit.omnibox.OmniboxFacility;
import org.chromium.chrome.test.transit.page.PageStation;

import java.util.List;

/**
 * The New Tab Page screen, with an omnibox, most visited tiles, and the Feed instead of the
 * WebContents.
 */
public class RegularNewTabPageStation extends PageStation {
    public static final ViewSpec SEARCH_LOGO = viewSpec(withId(R.id.search_provider_logo));
    public static final ViewSpec SEARCH_BOX = viewSpec(withId(R.id.search_box));
    public static final ViewSpec MOST_VISITED_TILES_CONTAINER =
            viewSpec(withId(R.id.mv_tiles_container));

    protected <T extends RegularNewTabPageStation> RegularNewTabPageStation(Builder<T> builder) {
        super(builder.withIncognito(false));
    }

    public static Builder<RegularNewTabPageStation> newBuilder() {
        return new Builder<>(RegularNewTabPageStation::new);
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        super.declareElements(elements);

        elements.declareElementFactory(
                mActivityElement,
                delayedElements -> {
                    if (mActivityElement.get().isTablet()) {
                        delayedElements.declareView(URL_BAR);
                    } else {
                        delayedElements.declareNoView(URL_BAR);
                    }
                });

        elements.declareView(SEARCH_LOGO);
        elements.declareView(SEARCH_BOX);
        elements.declareView(MOST_VISITED_TILES_CONTAINER);

        elements.declareEnterCondition(new NtpLoadedCondition(mPageLoadedSupplier));
    }

    /** Opens the app menu by pressing the toolbar "..." button */
    public RegularNewTabPageAppMenuFacility openAppMenu() {
        return enterFacilitySync(new RegularNewTabPageAppMenuFacility(), MENU_BUTTON::click);
    }

    /**
     * Checks MVTs exist and returns an {@param MvtsFacility} with interactions with Most Visited
     * Tiles.
     *
     * @param siteSuggestions the expected SiteSuggestions to be displayed. Use fakes ones for
     *     testing.
     */
    public MvtsFacility focusOnMvts(List<SiteSuggestion> siteSuggestions) {
        // Assume MVTs are on the screen; if this assumption changes, make sure to scroll to them.
        return enterFacilitySync(new MvtsFacility(siteSuggestions), /* trigger= */ null);
    }

    /** Click the URL bar to enter the Omnibox. */
    public Pair<OmniboxFacility, SoftKeyboardFacility> openOmnibox(
            FakeOmniboxSuggestions fakeSuggestions) {
        OmniboxFacility omniboxFacility =
                new OmniboxFacility(/* incognito= */ false, fakeSuggestions);
        SoftKeyboardFacility softKeyboard = new SoftKeyboardFacility(mActivityElement);
        enterFacilitiesSync(List.of(omniboxFacility, softKeyboard), SEARCH_BOX::click);
        return Pair.create(omniboxFacility, softKeyboard);
    }
}
