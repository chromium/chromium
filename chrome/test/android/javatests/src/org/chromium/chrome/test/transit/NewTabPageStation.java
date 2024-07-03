// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.chromium.base.test.transit.ViewElement.scopedViewElement;

import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.test.transit.ntp.MvtsFacility;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.List;

/**
 * The New Tab Page screen, with an omnibox, most visited tiles, and the Feed instead of the
 * WebContents.
 */
public class NewTabPageStation extends PageStation {
    public static final ViewElement SEARCH_LOGO =
            scopedViewElement(withId(R.id.search_provider_logo));
    public static final ViewElement SEARCH_BOX = scopedViewElement(withId(R.id.search_box));
    public static final ViewElement MOST_VISITED_TILES_CONTAINER =
            scopedViewElement(withId(R.id.mv_tiles_container));

    protected <T extends NewTabPageStation> NewTabPageStation(Builder<T> builder) {
        super(builder.withIncognito(false));
    }

    public static Builder<NewTabPageStation> newBuilder() {
        return new Builder<>(NewTabPageStation::new);
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        super.declareElements(elements);

        boolean isTablet = DeviceFormFactor.isTablet();

        // TODO(crbug.com/40267786): On android_32_google_apis_x64_foldable these elements do not
        // appear or appear unreliably when a keyboard is attached, which is the case for local
        // development and in bots.
        if (!isTablet) {
            elements.declareView(SEARCH_LOGO);
            elements.declareView(SEARCH_BOX);
            elements.declareView(MOST_VISITED_TILES_CONTAINER);
        }

        elements.declareEnterCondition(new NtpLoadedCondition(mPageLoadedSupplier));
    }

    /** Opens the app menu by pressing the toolbar "..." button */
    public NewTabPageRegularAppMenuFacility openAppMenu() {
        return enterFacilitySync(
                new NewTabPageRegularAppMenuFacility(this), () -> MENU_BUTTON.perform(click()));
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
        return enterFacilitySync(new MvtsFacility(this, siteSuggestions), /* trigger= */ null);
    }
}
