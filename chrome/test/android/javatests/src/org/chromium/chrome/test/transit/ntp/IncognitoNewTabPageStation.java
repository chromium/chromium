// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.ntp;

import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.chromium.base.test.transit.ViewSpec.viewSpec;

import android.util.Pair;

import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.R;
import org.chromium.chrome.test.transit.SoftKeyboardFacility;
import org.chromium.chrome.test.transit.omnibox.FakeOmniboxSuggestions;
import org.chromium.chrome.test.transit.omnibox.OmniboxFacility;
import org.chromium.chrome.test.transit.page.PageStation;

import java.util.List;

/** The Incognito New Tab Page screen, with text about Incognito mode. */
public class IncognitoNewTabPageStation extends PageStation {
    private ViewElement mUrlBar;

    protected <T extends IncognitoNewTabPageStation> IncognitoNewTabPageStation(
            Builder<T> builder) {
        super(builder.withIncognito(true));
    }

    public static Builder<IncognitoNewTabPageStation> newBuilder() {
        return new Builder<>(IncognitoNewTabPageStation::new);
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        super.declareElements(elements);
        mUrlBar = elements.declareView(URL_BAR);
        elements.declareView(viewSpec(withId(R.id.new_tab_incognito_icon)));
        elements.declareView(viewSpec(withText("You’ve gone Incognito")));
        elements.declareEnterCondition(new NtpLoadedCondition(mPageLoadedSupplier));
    }

    /** Opens the app menu by pressing the toolbar "..." button */
    public IncognitoNewTabPageAppMenuFacility openAppMenu() {
        return enterFacilitySync(
                new IncognitoNewTabPageAppMenuFacility(), mMenuButton.clickTrigger());
    }

    /** Click the URL bar to enter the Omnibox. */
    public Pair<OmniboxFacility, SoftKeyboardFacility> openOmnibox(
            FakeOmniboxSuggestions fakeSuggestions) {
        OmniboxFacility omniboxFacility =
                new OmniboxFacility(/* incognito= */ true, fakeSuggestions);
        SoftKeyboardFacility softKeyboard = new SoftKeyboardFacility();
        enterFacilitiesSync(List.of(omniboxFacility, softKeyboard), mUrlBar.clickTrigger());
        return Pair.create(omniboxFacility, softKeyboard);
    }
}
