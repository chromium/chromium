// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.omnibox;

import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;

import static org.hamcrest.CoreMatchers.instanceOf;

import static org.chromium.base.test.transit.ViewSpec.viewSpec;

import androidx.test.espresso.Espresso;
import androidx.test.espresso.action.ViewActions;

import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.base.test.transit.ViewSpec;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.page.PageStation;
import org.chromium.components.browser_ui.widget.scrim.ScrimView;

/** Represents the Omnibox focused state showing the URL bar and accepting keyboard input. */
public class OmniboxFacility extends Facility<PageStation> {
    public static final ViewSpec SCRIM = viewSpec(instanceOf(ScrimView.class));
    public static final ViewSpec STATUS_ICON = viewSpec(withId(R.id.location_bar_status_icon));
    public static final ViewSpec URL_FIELD = viewSpec(withId(R.id.url_bar));
    public static final ViewSpec ACTION_CONTAINER = viewSpec(withId(R.id.url_action_container));

    public static final ViewSpec MIC_BUTTON =
            viewSpec(withId(R.id.mic_button), withParent(ACTION_CONTAINER.getViewMatcher()));
    public static final ViewSpec DELETE_BUTTON =
            viewSpec(withId(R.id.delete_button), withParent(ACTION_CONTAINER.getViewMatcher()));
    private final boolean mIncognito;
    private final FakeOmniboxSuggestions mFakeSuggestions;

    public OmniboxFacility(boolean incognito, FakeOmniboxSuggestions fakeSuggestions) {
        assert fakeSuggestions != null : "Tests should not rely on server results.";
        mIncognito = incognito;
        mFakeSuggestions = fakeSuggestions;
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        elements.declareView(SCRIM);

        // Unscoped elements exist in PageStations too.
        // Action buttons are 71% displayed in tablets (though the actual image is fully displayed).
        if (!mIncognito) {
            // Regular tab
            elements.declareView(STATUS_ICON, ViewElement.unscopedOption());
            elements.declareView(URL_FIELD, ViewElement.unscopedOption());
            elements.declareView(
                    ACTION_CONTAINER,
                    ViewElement.newOptions().unscoped().displayingAtLeast(50).build());
            elements.declareView(MIC_BUTTON, ViewElement.displayingAtLeastOption(50));
            elements.declareNoView(DELETE_BUTTON);
        } else {
            if (mHostStation.getActivity().isTablet()) {
                // Incognito tab in tablet
                elements.declareView(STATUS_ICON, ViewElement.unscopedOption());
                elements.declareView(URL_FIELD, ViewElement.unscopedOption());
                elements.declareNoView(ACTION_CONTAINER);
                elements.declareNoView(MIC_BUTTON);
                elements.declareNoView(DELETE_BUTTON);
            } else {
                // Incognito tab in phone
                elements.declareNoView(STATUS_ICON);
                elements.declareView(URL_FIELD, ViewElement.unscopedOption());
                elements.declareNoView(ACTION_CONTAINER);
                elements.declareNoView(MIC_BUTTON);
                elements.declareNoView(DELETE_BUTTON);
            }
        }
    }

    public FakeOmniboxSuggestions getFakeSuggestions() {
        return mFakeSuggestions;
    }

    /** Enter text into the omnibox. */
    public OmniboxEnteredTextFacility typeText(String textToTypeAndExpect) {
        return mHostStation.enterFacilitySync(
                new OmniboxEnteredTextFacility(this, textToTypeAndExpect),
                () -> URL_FIELD.perform(ViewActions.typeText(textToTypeAndExpect)));
    }

    /** Press back expecting the Omnibox to be closed. */
    public void pressBackToClose() {
        mHostStation.exitFacilitySync(this, Espresso::pressBack);
    }
}
