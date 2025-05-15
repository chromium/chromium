// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.ntp;

import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.chromium.base.test.transit.Condition.whether;

import android.util.Pair;
import android.view.View;

import org.chromium.base.test.transit.Element;
import org.chromium.base.test.transit.SimpleConditions;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ntp.IncognitoNewTabPage;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.test.transit.SoftKeyboardFacility;
import org.chromium.chrome.test.transit.omnibox.FakeOmniboxSuggestions;
import org.chromium.chrome.test.transit.omnibox.OmniboxFacility;
import org.chromium.chrome.test.transit.page.NativePageCondition;
import org.chromium.chrome.test.transit.page.PageStation;
import org.chromium.components.embedder_support.util.UrlConstants;

import java.util.List;

/** The Incognito New Tab Page screen, with text about Incognito mode. */
public class IncognitoNewTabPageStation extends PageStation {
    public ViewElement<UrlBar> urlBarElement;
    public ViewElement<View> iconElement;
    public ViewElement<View> goneIncognitoTextElement;
    public Element<IncognitoNewTabPage> nativePageElement;

    protected <T extends IncognitoNewTabPageStation> IncognitoNewTabPageStation(
            Builder<T> builder) {
        super(builder.withIncognito(true).withExpectedUrlSubstring(UrlConstants.NTP_URL));

        urlBarElement = declareView(URL_BAR);
        iconElement = declareView(withId(R.id.new_tab_incognito_icon));
        goneIncognitoTextElement = declareView(withText("You’ve gone Incognito"));
        nativePageElement =
                declareEnterConditionAsElement(
                        new NativePageCondition<>(IncognitoNewTabPage.class, loadedTabElement));
        declareEnterCondition(
                SimpleConditions.uiThreadCondition(
                        "Incognito NTP is loaded",
                        nativePageElement,
                        nativePage -> whether(nativePage.isLoadedForTests())));
    }

    public static Builder<IncognitoNewTabPageStation> newBuilder() {
        return new Builder<>(IncognitoNewTabPageStation::new);
    }

    /** Opens the app menu by pressing the toolbar "..." button */
    public IncognitoNewTabPageAppMenuFacility openAppMenu() {
        return enterFacilitySync(
                new IncognitoNewTabPageAppMenuFacility(), menuButtonElement.getClickTrigger());
    }

    /** Click the URL bar to enter the Omnibox. */
    public Pair<OmniboxFacility, SoftKeyboardFacility> openOmnibox(
            FakeOmniboxSuggestions fakeSuggestions) {
        OmniboxFacility omniboxFacility =
                new OmniboxFacility(/* incognito= */ true, fakeSuggestions);
        SoftKeyboardFacility softKeyboard = new SoftKeyboardFacility();
        enterFacilitiesSync(
                List.of(omniboxFacility, softKeyboard), urlBarElement.getClickTrigger());
        return Pair.create(omniboxFacility, softKeyboard);
    }
}
