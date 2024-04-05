// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.chromium.base.test.transit.ViewElement.scopedViewElement;

import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.ConditionStatus;
import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.Trip;
import org.chromium.base.test.transit.UiThreadCondition;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;

/** The tab switcher screen showing regular tabs. */
public class RegularTabSwitcherStation extends TabSwitcherStation {

    public static final ViewElement EMPTY_STATE_TEXT =
            scopedViewElement(withText(R.string.tabswitcher_no_tabs_empty_state));

    public RegularTabSwitcherStation(ChromeTabbedActivityTestRule chromeTabbedActivityTestRule) {
        super(chromeTabbedActivityTestRule, /* incognito= */ false);
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        super.declareElements(elements);

        Condition noRegularTabsExist =
                new UiThreadCondition() {
                    @Override
                    public ConditionStatus check() {
                        int regularTabCount =
                                mChromeTabbedActivityTestRule.tabsCount(/* incognito= */ false);
                        return whether(regularTabCount == 0, "regular tabs: %d", regularTabCount);
                    }

                    @Override
                    public String buildDescription() {
                        return "No regular tabs exist";
                    }
                };
        elements.declareViewIf(EMPTY_STATE_TEXT, noRegularTabsExist);

        Condition incognitoTabsExist =
                new UiThreadCondition() {
                    @Override
                    public ConditionStatus check() {
                        int incognitoTabCount =
                                mChromeTabbedActivityTestRule.tabsCount(/* incognito= */ true);
                        return whether(
                                incognitoTabCount > 0, "incognito tabs: %d", incognitoTabCount);
                    }

                    @Override
                    public String buildDescription() {
                        return "Incognito tabs exist";
                    }
                };
        elements.declareViewIf(INCOGNITO_TOGGLE_TABS, incognitoTabsExist);
        elements.declareViewIf(REGULAR_TOGGLE_TAB_BUTTON, incognitoTabsExist);
        elements.declareViewIf(INCOGNITO_TOGGLE_TAB_BUTTON, incognitoTabsExist);
    }

    public IncognitoTabSwitcherStation selectIncognitoTabList() {
        IncognitoTabSwitcherStation tabSwitcher =
                new IncognitoTabSwitcherStation(mChromeTabbedActivityTestRule);
        return Trip.travelSync(
                this, tabSwitcher, () -> INCOGNITO_TOGGLE_TAB_BUTTON.perform(click()));
    }
}
