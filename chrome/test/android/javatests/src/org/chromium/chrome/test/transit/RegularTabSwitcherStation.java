// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.chromium.base.test.transit.ViewElement.scopedViewElement;

import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.test.R;

/** The tab switcher screen showing regular tabs. */
public class RegularTabSwitcherStation extends TabSwitcherStation {

    public static final ViewElement EMPTY_STATE_TEXT =
            scopedViewElement(withText(R.string.tabswitcher_no_tabs_empty_state));

    public <T extends RegularTabSwitcherStation> RegularTabSwitcherStation(Builder<T> builder) {
        super(builder.withIncognito(false));
    }

    public static Builder<RegularTabSwitcherStation> newBuilder() {
        return new Builder<>(RegularTabSwitcherStation::new);
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        super.declareElements(elements);

        if (!mHasRegularTabs) {
            elements.declareView(EMPTY_STATE_TEXT);
        }

        if (mHasIncognitoTabs) {
            elements.declareView(INCOGNITO_TOGGLE_TABS);
            elements.declareView(REGULAR_TOGGLE_TAB_BUTTON);
            elements.declareView(INCOGNITO_TOGGLE_TAB_BUTTON);
        }
    }

    public IncognitoTabSwitcherStation selectIncognitoTabList() {
        IncognitoTabSwitcherStation tabSwitcher =
                IncognitoTabSwitcherStation.newBuilder().initFrom(this).build();
        return travelToSync(tabSwitcher, () -> INCOGNITO_TOGGLE_TAB_BUTTON.perform(click()));
    }
}
