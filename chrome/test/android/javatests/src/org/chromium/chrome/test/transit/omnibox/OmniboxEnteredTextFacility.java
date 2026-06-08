// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.omnibox;

import static androidx.test.espresso.matcher.ViewMatchers.withText;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.Station;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.omnibox.OmniboxCapabilities;

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
            } else if (!OmniboxCapabilities.isDesktopPlatform()) {
                // Non-desktop platform devices should show mic button.
                // Mic behaviour on desktop platform devices is still WIP.
                // TODO(crbug.com/521341182): Revisit the mic visibility check when desktop platform
                // behaviour is more stable.
                declareEnterCondition(omniboxFacility.micButtonElement.present());
            }
        } else {
            boolean hasDesktopExperience =
                    ThreadUtils.runOnUiThreadBlocking(
                            () ->
                                    OmniboxCapabilities.hasDesktopExperience(
                                            ContextUtils.getApplicationContext()));
            // Desktop experience hides the delete button in conventional, non-AI mode.
            if (hasDesktopExperience) {
                declareEnterCondition(omniboxFacility.deleteButtonElement.absent());
            } else {
                declareEnterCondition(omniboxFacility.deleteButtonElement.present());
            }
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
                        () -> {
                            Profile profile =
                                    mOmniboxFacility.getHostStation().getTab().getProfile();
                            mOmniboxFacility
                                    .getFakeSuggestions()
                                    .simulateAutocompleteSuggestion(profile, mText, autocompleted);
                        })
                .exitFacilityAnd()
                .enterFacility(
                        new OmniboxEnteredTextFacility(mOmniboxFacility, mText + autocompleted));
    }

    /** Clear text in the omnibox. */
    public OmniboxEnteredTextFacility clearText() {
        return mOmniboxFacility.setText("");
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
