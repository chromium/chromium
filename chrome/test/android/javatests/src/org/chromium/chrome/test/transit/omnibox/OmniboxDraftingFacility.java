// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.omnibox;

import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.CoreMatchers.allOf;

import static org.chromium.base.test.transit.ViewSpec.viewSpec;

import android.view.View;

import org.chromium.base.test.transit.ViewSpec;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.LocationBarLayout;

/** Represents the Omnibox when focused in drafting mode (DisplayState.DRAFTING). */
public class OmniboxDraftingFacility extends OmniboxEnteredTextFacility {
    public static final ViewSpec<LocationBarLayout> LOCATION_BAR_POPPED_OUT =
            viewSpec(
                    LocationBarLayout.class,
                    allOf(
                            withId(R.id.location_bar),
                            isDescendantOfA(withId(R.id.omnibox_suggestions_container))));

    public static final ViewSpec<View> SUGGESTIONS_DROPDOWN =
            viewSpec(allOf(withId(R.id.omnibox_suggestions_dropdown), isDisplayed()));

    public OmniboxDraftingFacility(OmniboxFacility omniboxFacility, String text) {
        super(omniboxFacility, text);

        declareNoView(LOCATION_BAR_POPPED_OUT);
        declareNoView(SUGGESTIONS_DROPDOWN);
    }
}
