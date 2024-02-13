// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import static androidx.test.espresso.action.ViewActions.click;

import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.Trip;
import org.chromium.base.test.transit.UiThreadCondition;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;

/** The tab switcher screen showing regular tabs. */
public class RegularTabSwitcherStation extends TabSwitcherStation {

    public RegularTabSwitcherStation(ChromeTabbedActivityTestRule chromeTabbedActivityTestRule) {
        super(chromeTabbedActivityTestRule, /* incognito= */ false);
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        super.declareElements(elements);

        TabModelSelector tabModelSelector =
                mChromeTabbedActivityTestRule.getActivity().getTabModelSelector();

        Condition noRegularTabsExist =
                new UiThreadCondition() {
                    @Override
                    public boolean check() {
                        return tabModelSelector.getModel(false).getCount() == 0;
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
                    public boolean check() {
                        return tabModelSelector.getModel(true).getCount() > 0;
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
                this, tabSwitcher, (t) -> INCOGNITO_TOGGLE_TAB_BUTTON.perform(click()));
    }
}
