// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.chromium.base.test.transit.ViewElement.sharedViewElement;

import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.R;

/**
 * The New Tab Page screen, with an omnibox, most visited tiles, and the Feed instead of the
 * WebContents.
 */
public class NewTabPageStation extends PageStation {
    public ViewElement SEARCH_LOGO = sharedViewElement(withId(R.id.search_provider_logo));
    public ViewElement SEARCH_BOX = sharedViewElement(withId(R.id.search_box));
    public ViewElement MOST_VISITED_TILES = sharedViewElement(withId(R.id.mv_tiles_container));

    protected <T extends NewTabPageStation> NewTabPageStation(Builder<T> builder) {
        super(builder.withIncognito(false));
    }

    public static Builder<NewTabPageStation> newBuilder() {
        return new Builder<>(NewTabPageStation::new);
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        super.declareElements(elements);

        boolean isTablet = mChromeTabbedActivityTestRule.getActivity().isTablet();

        // TODO(crbug.com/40267786): On generic_android32_foldable these elements do not appear or
        // appear unreliably when a keyboard is attached, which is the case for local development
        // and in bots.
        if (!isTablet) {
            elements.declareView(SEARCH_LOGO);
            elements.declareView(SEARCH_BOX);
            elements.declareView(MOST_VISITED_TILES);
        }

        elements.declareEnterCondition(new NtpLoadedCondition(mPageLoadedCondition));
    }

    /** Opens the app menu by pressing the toolbar "..." button */
    public NewTabPageRegularAppMenuFacility openAppMenu() {
        return Facility.enterSync(
                new NewTabPageRegularAppMenuFacility(this), () -> MENU_BUTTON.perform(click()));
    }
}
