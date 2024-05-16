// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.chromium.base.test.transit.ViewElement.sharedViewElement;

import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.R;

/** The Incognito New Tab Page screen, with text about Incognito mode. */
public class IncognitoNewTabPageStation extends PageStation {
    public ViewElement ICON = sharedViewElement(withId(R.id.new_tab_incognito_icon));
    public ViewElement GONE_INCOGNITO_TEXT = sharedViewElement(withText("You’ve gone Incognito"));

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
        elements.declareView(ICON);
        elements.declareView(GONE_INCOGNITO_TEXT);
        elements.declareEnterCondition(new NtpLoadedCondition(mPageLoadedCondition));
    }

    /** Opens the app menu by pressing the toolbar "..." button */
    public NewTabPageIncognitoAppMenuFacility openAppMenu() {
        return enterFacilitySync(
                new NewTabPageIncognitoAppMenuFacility(this), () -> MENU_BUTTON.perform(click()));
    }
}
