// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.chromium.base.test.transit.ViewElement.sharedViewElement;

import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;

/** Regular tab switcher pane station. */
public class HubTabSwitcherStation extends HubTabSwitcherBaseStation {
    public static final ViewElement EMPTY_STATE_TEXT =
            sharedViewElement(withText(R.string.tabswitcher_no_tabs_empty_state));

    /**
     * @param chromeTabbedActivityTestRule The activity rule under test.
     */
    public HubTabSwitcherStation(ChromeTabbedActivityTestRule chromeTabbedActivityTestRule) {
        super(chromeTabbedActivityTestRule, /* isIncognito= */ false);
    }

    @Override
    public @PaneId int getPaneId() {
        return PaneId.TAB_SWITCHER;
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        super.declareElements(elements);

        elements.declareViewIf(
                EMPTY_STATE_TEXT,
                TabModelConditions.noRegularTabsExist(mTabModelSelectorCondition));
    }
}
