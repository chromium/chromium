// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.ntp;

import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.chromium.base.test.transit.Condition.whether;
import static org.chromium.chrome.browser.url_constants.UrlConstantResolver.getOriginalNativeNtpUrl;

import android.view.View;

import org.chromium.base.test.transit.Element;
import org.chromium.base.test.transit.SimpleConditions;
import org.chromium.base.test.transit.TripBuilder;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ntp.IncognitoNewTabPage;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.test.transit.page.CtaPageStation;
import org.chromium.chrome.test.transit.page.NativePageCondition;

/** The Incognito New Tab Page screen, with text about Incognito mode. */
public class IncognitoNewTabPageStation extends CtaPageStation {
    public ViewElement<UrlBar> urlBarElement;
    public ViewElement<View> iconElement;
    public ViewElement<View> goneIncognitoTextElement;
    public Element<IncognitoNewTabPage> nativePageElement;

    public IncognitoNewTabPageStation(Config config) {
        super(config.withIncognito(true).withExpectedUrlSubstring(getOriginalNativeNtpUrl()));

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
        return menuButtonElement.clickTo().enterFacility(new IncognitoNewTabPageAppMenuFacility());
    }

    @Override
    protected TripBuilder clickUrlBarOrSearchBarTo() {
        return urlBarElement.clickTo();
    }
}
