// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.omnibox;

import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.CoreMatchers.instanceOf;

import static org.chromium.base.test.transit.ViewSpec.viewSpec;

import android.view.View;

import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.base.test.transit.ViewSpec;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.page.CtaPageStation;
import org.chromium.components.browser_ui.widget.scrim.ScrimView;

/** Represents the Omnibox focused state showing the URL bar and accepting keyboard input. */
public class OmniboxFacility extends Facility<CtaPageStation> {
    public static final ViewSpec<View> STATUS_ICON =
            viewSpec(withId(R.id.location_bar_status_icon));
    public static final ViewSpec<UrlBar> URL_FIELD = viewSpec(UrlBar.class, withId(R.id.url_bar));
    public static final ViewSpec<View> LOCATION_BAR = viewSpec(withId(R.id.location_bar));
    public static final ViewSpec<View> MIC_BUTTON =
            LOCATION_BAR.descendant(withId(R.id.mic_button));
    public static final ViewSpec<View> DELETE_BUTTON =
            LOCATION_BAR.descendant(withId(R.id.delete_button));
    private final boolean mIncognito;
    private final FakeOmniboxSuggestions mFakeSuggestions;
    public ViewElement<UrlBar> urlBarElement;
    public ViewElement<View> actionContainerElement;
    public ViewElement<View> micButtonElement;

    public OmniboxFacility(boolean incognito, FakeOmniboxSuggestions fakeSuggestions) {
        assert fakeSuggestions != null : "Tests should not rely on server results.";
        mIncognito = incognito;
        mFakeSuggestions = fakeSuggestions;
    }

    @Override
    public void declareExtraElements() {
        declareView(instanceOf(ScrimView.class));

        // Unscoped elements exist in PageStations too.
        //
        // Action buttons are 71% displayed in tablets (though the actual image is fully displayed).
        //
        // TODO(crbug.com/416324280): Add status icon back with id R.id.location_bar_status_icon as
        // optional ViewElement.
        if (!mIncognito) {
            // Regular tab
            urlBarElement = declareView(URL_FIELD, ViewElement.unscopedOption());
            micButtonElement =
                    declareView(
                            MIC_BUTTON,
                            ViewElement.newOptions().unscoped().displayingAtLeast(50).build());
            declareNoView(DELETE_BUTTON);
        } else {
            // Incognito tab
            urlBarElement = declareView(URL_FIELD, ViewElement.unscopedOption());
            declareNoView(MIC_BUTTON);
            declareNoView(DELETE_BUTTON);
        }
    }

    public FakeOmniboxSuggestions getFakeSuggestions() {
        return mFakeSuggestions;
    }

    /** Enter text into the omnibox. */
    public OmniboxEnteredTextFacility typeText(String textToTypeAndExpect) {
        return urlBarElement
                .typeTextTo(textToTypeAndExpect)
                .enterFacility(new OmniboxEnteredTextFacility(this, textToTypeAndExpect));
    }
}
