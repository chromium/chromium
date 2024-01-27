// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;

import static org.hamcrest.CoreMatchers.allOf;

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
        // TODO(crbug.com/1521184): If the view is declared as owned with declareViewIf(), the empty
        // state text is still
        // thought to exist by DoesNotExistCondition, even though it's not actually displayed.
        elements.declareUnownedViewIf(EMPTY_STATE_TEXT, noRegularTabsExist);

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
        elements.declareUnownedViewIf(INCOGNITO_TOGGLE_TABS, incognitoTabsExist);
        elements.declareUnownedViewIf(REGULAR_TOGGLE_TAB_BUTTON, incognitoTabsExist);
        elements.declareUnownedViewIf(INCOGNITO_TOGGLE_TAB_BUTTON, incognitoTabsExist);
    }

    public IncognitoTabSwitcherStation selectIncognitoTabList() {
        IncognitoTabSwitcherStation tabSwitcher =
                new IncognitoTabSwitcherStation(mChromeTabbedActivityTestRule);
        return Trip.travelSync(
                this,
                tabSwitcher,
                (t) -> onView(allOf(isDisplayed(), INCOGNITO_TOGGLE_TAB_BUTTON)).perform(click()));
    }
}
