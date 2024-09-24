// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.omnibox;

import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.chromium.base.test.transit.ViewSpec.viewSpec;

import androidx.test.espresso.action.ViewActions;

import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.ViewElement;

/**
 * Represents test entered into the Omnibox.
 *
 * <p>TODO(crbug.com/345808144): Make this a child of OmniboxFacility when Facilities can have
 * children like Stations.
 */
public class OmniboxEnteredTextFacility extends Facility<Station> {
    private final OmniboxFacility mOmniboxFacility;
    private final String mText;

    public OmniboxEnteredTextFacility(OmniboxFacility omniboxFacility, String text) {
        mOmniboxFacility = omniboxFacility;
        mText = text;
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        elements.declareView(viewSpec(OmniboxFacility.URL_FIELD.getViewMatcher(), withText(mText)));
        elements.declareView(
                OmniboxFacility.DELETE_BUTTON, ViewElement.displayingAtLeastOption(50));
        elements.declareNoView(OmniboxFacility.MIC_BUTTON);
    }

    /** Enter text into the omnibox. */
    public OmniboxEnteredTextFacility typeText(String textToType, String textToExpect) {
        return mHostStation.swapFacilitySync(
                this,
                new OmniboxEnteredTextFacility(mOmniboxFacility, textToExpect),
                () -> OmniboxFacility.URL_FIELD.perform(ViewActions.typeText(textToType)));
    }

    /** Simulate autocomplete suggestion received from the server. */
    public OmniboxEnteredTextFacility simulateAutocomplete(String autocompleted) {
        return mHostStation.swapFacilitySync(
                this,
                new OmniboxEnteredTextFacility(mOmniboxFacility, mText + autocompleted),
                () ->
                        mOmniboxFacility
                                .getFakeSuggestions()
                                .simulateAutocompleteSuggestion(mText, autocompleted));
    }

    /** Click the delete button to erase the text entered. */
    public void clickDelete() {
        mHostStation.exitFacilitySync(this, OmniboxFacility.DELETE_BUTTON::forgivingClick);
    }
}
