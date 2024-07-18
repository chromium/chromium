// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.hub;

import static androidx.test.espresso.matcher.ViewMatchers.isSelected;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;

import static org.chromium.base.test.transit.ViewElement.scopedViewElement;

import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.test.R;

/** Regular tab switcher pane station. */
public class RegularTabSwitcherStation extends TabSwitcherStation {
    public static final ViewElement EMPTY_STATE_TEXT =
            scopedViewElement(withText(R.string.tabswitcher_no_tabs_empty_state));
    public static final ViewElement SELECTED_REGULAR_TOGGLE_TAB_BUTTON =
            scopedViewElement(allOf(REGULAR_TOGGLE_TAB_BUTTON.getViewMatcher(), isSelected()));

    public RegularTabSwitcherStation(boolean regularTabsExist, boolean incognitoTabsExist) {
        super(/* isIncognito= */ false, regularTabsExist, incognitoTabsExist);
    }

    @Override
    public @PaneId int getPaneId() {
        return PaneId.TAB_SWITCHER;
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        super.declareElements(elements);
        if (mIncognitoTabsExist) {
            elements.declareView(SELECTED_REGULAR_TOGGLE_TAB_BUTTON);
        }
        if (!mRegularTabsExist) {
            elements.declareView(EMPTY_STATE_TEXT);
        }
    }
}
