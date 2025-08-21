// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.ntp;

import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.chromium.base.test.transit.Condition.whether;

import android.util.Pair;
import android.view.View;

import org.chromium.base.test.transit.Element;
import org.chromium.base.test.transit.SimpleConditions;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.test.transit.SoftKeyboardFacility;
import org.chromium.chrome.test.transit.omnibox.FakeOmniboxSuggestions;
import org.chromium.chrome.test.transit.omnibox.OmniboxFacility;
import org.chromium.chrome.test.transit.page.CtaPageStation;
import org.chromium.chrome.test.transit.page.NativePageCondition;
import org.chromium.components.embedder_support.util.UrlConstants;

import java.util.Collections;
import java.util.List;
import java.util.Set;

/**
 * The New Tab Page screen, with an omnibox, most visited tiles, and the Feed instead of the
 * WebContents.
 */
public class RegularNewTabPageStation extends CtaPageStation {
    public ViewElement<View> searchBoxElement;
    public ViewElement<UrlBar> urlBarElement;
    public ViewElement<View> logoElement;
    public Element<NewTabPage> nativePageElement;

    public RegularNewTabPageStation(Config config) {
        super(config.withIncognito(false).withExpectedUrlSubstring(UrlConstants.NTP_URL));

        declareElementFactory(
                mActivityElement,
                delayedElements -> {
                    if (mActivityElement.value().isTablet()) {
                        urlBarElement = delayedElements.declareView(URL_BAR);
                    } else {
                        delayedElements.declareNoView(URL_BAR);
                    }
                });

        logoElement = declareView(withId(R.id.search_provider_logo));
        searchBoxElement = declareView(withId(R.id.search_box));

        nativePageElement =
                declareEnterConditionAsElement(
                        new NativePageCondition<>(NewTabPage.class, loadedTabElement));
        declareEnterCondition(
                SimpleConditions.uiThreadCondition(
                        "Regular NTP is loaded",
                        nativePageElement,
                        nativePage -> whether(nativePage.isLoadedForTests())));
    }

    public static Builder<RegularNewTabPageStation> newBuilder() {
        return new Builder<>(RegularNewTabPageStation::new);
    }

    /** Opens the app menu by pressing the toolbar "..." button */
    public RegularNewTabPageAppMenuFacility openAppMenu() {
        return menuButtonElement.clickTo().enterFacility(new RegularNewTabPageAppMenuFacility());
    }

    /**
     * Checks MVTs exist and returns an {@param MvtsFacility} with interactions with Most Visited
     * Tiles.
     *
     * @param siteSuggestions the expected SiteSuggestions to be displayed. Use fakes ones for
     *     testing.
     * @param separatorIndices the indices of separators between tiles.
     */
    public MvtsFacility focusOnMvts(
            List<SiteSuggestion> siteSuggestions, Set<Integer> separatorIndices) {
        // Assume MVTs are on the screen; if this assumption changes, make sure to scroll to them.
        return noopTo().enterFacility(new MvtsFacility(siteSuggestions, separatorIndices));
    }

    /** Same as {@link #focusOnMvts(List, Set)} expecting no separatorIndices. */
    public MvtsFacility focusOnMvts(List<SiteSuggestion> siteSuggestions) {
        return focusOnMvts(siteSuggestions, Collections.emptySet());
    }

    /** Click the URL bar to enter the Omnibox. */
    public Pair<OmniboxFacility, SoftKeyboardFacility> openOmnibox(
            FakeOmniboxSuggestions fakeSuggestions) {
        OmniboxFacility omniboxFacility =
                new OmniboxFacility(/* incognito= */ false, fakeSuggestions);
        SoftKeyboardFacility softKeyboard = new SoftKeyboardFacility();
        searchBoxElement.clickTo().enterFacilities(omniboxFacility, softKeyboard);
        return Pair.create(omniboxFacility, softKeyboard);
    }
}
