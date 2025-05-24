// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.omnibox;

import static androidx.test.espresso.matcher.ViewMatchers.withText;

import android.view.View;

import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.ViewElement;

/**
 * Represents test entered into the Omnibox.
 *
 * <p>TODO(crbug.com/345808144): Make this a child of OmniboxFacility when Facilities can have
 * children like Stations.
 */
public class OmniboxEnteredTextFacility extends Facility<Station<?>> {
    private final OmniboxFacility mOmniboxFacility;
    private final String mText;
    public ViewElement<View> urlBarElement;
    public ViewElement<View> deleteButtonElement;

    public OmniboxEnteredTextFacility(OmniboxFacility omniboxFacility, String text) {
        mOmniboxFacility = omniboxFacility;
        mText = text;

        urlBarElement = declareView(OmniboxFacility.URL_FIELD.and(withText(mText)));
        deleteButtonElement =
                declareView(OmniboxFacility.DELETE_BUTTON, ViewElement.displayingAtLeastOption(50));
        declareNoView(OmniboxFacility.MIC_BUTTON);
    }

    /** Enter text into the omnibox. */
    public OmniboxEnteredTextFacility typeText(String textToType, String textToExpect) {
        return mHostStation.swapFacilitySync(
                this,
                new OmniboxEnteredTextFacility(mOmniboxFacility, textToExpect),
                urlBarElement.getTypeTextTrigger(textToType));
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
        mHostStation.exitFacilitySync(this, deleteButtonElement.getForgivingClickTrigger());
    }
}
