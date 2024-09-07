// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.hub;

import static androidx.test.espresso.matcher.ViewMatchers.isSelected;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.chromium.base.test.transit.ViewSpec.viewSpec;

import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.ViewSpec;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;

/** Regular tab switcher pane station. */
public class RegularTabSwitcherStation extends TabSwitcherStation {
    public static final ViewSpec EMPTY_STATE_TEXT =
            viewSpec(withText(R.string.tabswitcher_no_tabs_empty_state));
    public static final ViewSpec SELECTED_REGULAR_TOGGLE_TAB_BUTTON =
            viewSpec(REGULAR_TOGGLE_TAB_BUTTON.getViewMatcher(), isSelected());

    public RegularTabSwitcherStation(boolean regularTabsExist, boolean incognitoTabsExist) {
        super(/* isIncognito= */ false, regularTabsExist, incognitoTabsExist);
    }

    /**
     * Build a {@link RegularTabSwitcherStation} assuming the tab model doesn't change in the
     * transition.
     */
    public static RegularTabSwitcherStation from(TabModelSelector selector) {
        return new RegularTabSwitcherStation(
                selector.getModel(false).getCount() > 0, selector.getModel(true).getCount() > 0);
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

    /** Open a new tab using the New Tab action button. */
    public RegularNewTabPageStation openNewTab() {
        recheckActiveConditions();

        RegularNewTabPageStation page =
                RegularNewTabPageStation.newBuilder()
                        .withIsOpeningTabs(1)
                        .withIsSelectingTabs(1)
                        .build();

        return travelToSync(page, getNewTabButtonViewSpec()::click);
    }
}
