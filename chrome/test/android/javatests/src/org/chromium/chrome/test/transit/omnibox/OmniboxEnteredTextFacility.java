// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.omnibox;

import static androidx.test.espresso.matcher.ViewMatchers.withText;

import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.Station;

/**
 * Represents test entered into the Omnibox.
 *
 * <p>TODO(crbug.com/345808144): Make this a child of OmniboxFacility when Facilities can have
 * children like Stations.
 */
public class OmniboxEnteredTextFacility extends Facility<Station<?>> {
    private final OmniboxFacility mOmniboxFacility;
    private final String mText;

    public OmniboxEnteredTextFacility(OmniboxFacility omniboxFacility, String text) {
        mOmniboxFacility = omniboxFacility;
        mText = text;

        declareEnterCondition(omniboxFacility.urlBarElement.matches(withText(mText)));
        if (mText.isEmpty()) {
            declareEnterCondition(omniboxFacility.deleteButtonElement.absent());

            if (omniboxFacility.getHostStation().isIncognito()) {
                declareEnterCondition(omniboxFacility.micButtonElement.absent());
            } else {
                declareEnterCondition(omniboxFacility.micButtonElement.present());
            }
        } else {
            declareEnterCondition(omniboxFacility.deleteButtonElement.present());
            declareEnterCondition(omniboxFacility.micButtonElement.absent());
        }
    }

    /** Enter text into the omnibox. */
    public OmniboxEnteredTextFacility typeText(String textToType, String textToExpect) {
        return mOmniboxFacility
                .urlBarElement
                .typeTextTo(textToType)
                .exitFacilityAnd()
                .enterFacility(new OmniboxEnteredTextFacility(mOmniboxFacility, textToExpect));
    }

    /** Simulate autocomplete suggestion received from the server. */
    public OmniboxEnteredTextFacility simulateAutocomplete(String autocompleted) {
        return runTo(
                        () ->
                                mOmniboxFacility
                                        .getFakeSuggestions()
                                        .simulateAutocompleteSuggestion(mText, autocompleted))
                .exitFacilityAnd()
                .enterFacility(
                        new OmniboxEnteredTextFacility(mOmniboxFacility, mText + autocompleted));
    }

    /** Click the delete button to erase the text entered. */
    public OmniboxEnteredTextFacility clickDelete() {
        assert !mText.isEmpty();
        return mOmniboxFacility
                .deleteButtonElement
                .clickTo()
                .exitFacilityAnd(this)
                .enterFacility(new OmniboxEnteredTextFacility(mOmniboxFacility, ""));
    }
}
