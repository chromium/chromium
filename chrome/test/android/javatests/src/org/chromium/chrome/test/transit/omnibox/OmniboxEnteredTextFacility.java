// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.omnibox;

import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;

import static org.chromium.base.test.transit.ViewSpec.viewSpec;

import android.view.View;

import androidx.test.espresso.Espresso;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.ViewSpec;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.LocationBarLayout;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.omnibox.OmniboxCapabilities;

/**
 * Represents the Omnibox in a state where text has been entered in conventional mode.
 *
 * <p>TODO(crbug.com/345808144): Make this a child of OmniboxFacility when Facilities can have
 * children like Stations.
 */
public class OmniboxEnteredTextFacility extends Facility<Station<?>> {
    private final OmniboxFacility mOmniboxFacility;
    private final String mText;

    public static final ViewSpec<LocationBarLayout> LOCATION_BAR_POPPED_OUT =
            viewSpec(
                    LocationBarLayout.class,
                    allOf(
                            withId(R.id.location_bar),
                            isDescendantOfA(withId(R.id.omnibox_suggestions_container))));

    public static final ViewSpec<View> SUGGESTIONS_DROPDOWN =
            viewSpec(allOf(withId(R.id.omnibox_suggestions_dropdown), isDisplayed()));

    public OmniboxEnteredTextFacility(OmniboxFacility omniboxFacility, String text) {
        mOmniboxFacility = omniboxFacility;
        mText = text;

        declareEnterCondition(omniboxFacility.urlBarElement.matches(withText(mText)));
        if (mText.isEmpty()) {
            declareEnterCondition(omniboxFacility.deleteButtonElement.absent());
            if (OmniboxCapabilities.isDesktopPlatform()) {
                declareEnterCondition(omniboxFacility.micButtonElement.absent());
            } else {
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
    public OmniboxSuggestionsFacility simulateAutocomplete(String autocompleted) {
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
                        new OmniboxSuggestionsFacility(mOmniboxFacility, mText + autocompleted));
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

    /** Closes the keyboard and presses Back to exit the Omnibox facility. */
    public void pressBackToExit() {
        // The soft keyboard swallows the first back press if open.
        Espresso.closeSoftKeyboard();
        pressBackTo().exitFacilityAnd().exitFacility(mOmniboxFacility);
    }
}
