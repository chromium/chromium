// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.chromium.base.test.transit.ViewElement.viewElement;

import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.test.R;

/** The tab switcher screen showing regular tabs. */
public class RegularTabSwitcherStation extends TabSwitcherStation {

    public static final ViewElement EMPTY_STATE_TEXT =
            viewElement(withText(R.string.tabswitcher_no_tabs_empty_state));

    public RegularTabSwitcherStation() {
        super(/* incognito= */ false);
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        super.declareElements(elements);

        Condition noRegularTabsExist =
                TabModelConditions.noRegularTabsExist(mTabModelSelectorCondition);
        elements.declareViewIf(EMPTY_STATE_TEXT, noRegularTabsExist);

        Condition incognitoTabsExist =
                TabModelConditions.anyIncognitoTabsExist(mTabModelSelectorCondition);
        elements.declareViewIf(INCOGNITO_TOGGLE_TABS, incognitoTabsExist);
        elements.declareViewIf(REGULAR_TOGGLE_TAB_BUTTON, incognitoTabsExist);
        elements.declareViewIf(INCOGNITO_TOGGLE_TAB_BUTTON, incognitoTabsExist);
    }

    public IncognitoTabSwitcherStation selectIncognitoTabList() {
        IncognitoTabSwitcherStation tabSwitcher = new IncognitoTabSwitcherStation();
        return travelToSync(tabSwitcher, () -> INCOGNITO_TOGGLE_TAB_BUTTON.perform(click()));
    }
}
