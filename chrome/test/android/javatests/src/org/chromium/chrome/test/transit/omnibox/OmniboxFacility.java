// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.omnibox;

import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.CoreMatchers.instanceOf;

import static org.chromium.base.test.transit.ViewSpec.viewSpec;

import android.view.View;

import androidx.test.espresso.Espresso;

import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.base.test.transit.ViewSpec;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.page.PageStation;
import org.chromium.components.browser_ui.widget.scrim.ScrimView;

/** Represents the Omnibox focused state showing the URL bar and accepting keyboard input. */
public class OmniboxFacility extends Facility<PageStation> {
    public static final ViewSpec<View> STATUS_ICON =
            viewSpec(withId(R.id.location_bar_status_icon));
    public static final ViewSpec<UrlBar> URL_FIELD = viewSpec(UrlBar.class, withId(R.id.url_bar));
    public static final ViewSpec<View> ACTION_CONTAINER =
            viewSpec(withId(R.id.url_action_container));
    public static final ViewSpec<View> MIC_BUTTON =
            ACTION_CONTAINER.descendant(withId(R.id.mic_button));
    public static final ViewSpec<View> DELETE_BUTTON =
            ACTION_CONTAINER.descendant(withId(R.id.delete_button));
    private final boolean mIncognito;
    private final FakeOmniboxSuggestions mFakeSuggestions;
    public ViewElement<View> statusIconElement;
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
        // Action buttons are 71% displayed in tablets (though the actual image is fully displayed).
        if (!mIncognito) {
            // Regular tab
            statusIconElement = declareView(STATUS_ICON, ViewElement.unscopedOption());
            urlBarElement = declareView(URL_FIELD, ViewElement.unscopedOption());
            actionContainerElement =
                    declareView(
                            ACTION_CONTAINER,
                            ViewElement.newOptions().unscoped().displayingAtLeast(50).build());
            micButtonElement =
                    declareView(
                            MIC_BUTTON,
                            ViewElement.newOptions().unscoped().displayingAtLeast(50).build());
            declareNoView(DELETE_BUTTON);
        } else {
            if (mHostStation.getActivity().isTablet()) {
                // Incognito tab in tablet
                statusIconElement = declareView(STATUS_ICON, ViewElement.unscopedOption());
                urlBarElement = declareView(URL_FIELD, ViewElement.unscopedOption());
                declareNoView(ACTION_CONTAINER);
                declareNoView(MIC_BUTTON);
                declareNoView(DELETE_BUTTON);
            } else {
                // Incognito tab in phone
                declareNoView(STATUS_ICON);
                urlBarElement = declareView(URL_FIELD, ViewElement.unscopedOption());
                declareNoView(ACTION_CONTAINER);
                declareNoView(MIC_BUTTON);
                declareNoView(DELETE_BUTTON);
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
                urlBarElement.getTypeTextTrigger(textToTypeAndExpect));
    }

    /** Press back expecting the Omnibox to be closed. */
    public void pressBackToClose() {
        mHostStation.exitFacilitySync(this, Espresso::pressBack);
    }
}
