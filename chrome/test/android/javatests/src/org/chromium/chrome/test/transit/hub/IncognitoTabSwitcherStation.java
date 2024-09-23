// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.hub;

import static androidx.test.espresso.matcher.ViewMatchers.isSelected;

import static org.chromium.base.test.transit.ViewSpec.viewSpec;

import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.ViewSpec;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.transit.ntp.IncognitoNewTabPageStation;

/** Incognito tab switcher pane station. */
public class IncognitoTabSwitcherStation extends TabSwitcherStation {

    public static final ViewSpec SELECTED_INCOGNITO_TOGGLE_TAB_BUTTON =
            viewSpec(INCOGNITO_TOGGLE_TAB_BUTTON.getViewMatcher(), isSelected());

    public IncognitoTabSwitcherStation(boolean regularTabsExist, boolean incognitoTabsExist) {
        super(/* isIncognito= */ true, regularTabsExist, incognitoTabsExist);
    }

    /**
     * Build an {@link IncognitoTabSwitcherStation} assuming the tab model doesn't change in the
     * transition.
     */
    public static IncognitoTabSwitcherStation from(TabModelSelector selector) {
        return new IncognitoTabSwitcherStation(
                selector.getModel(false).getCount() > 0, selector.getModel(true).getCount() > 0);
    }

    @Override
    public @PaneId int getPaneId() {
        return PaneId.INCOGNITO_TAB_SWITCHER;
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        super.declareElements(elements);
        elements.declareView(SELECTED_INCOGNITO_TOGGLE_TAB_BUTTON);
    }

    /** Open a new tab using the New Tab action button. */
    public IncognitoNewTabPageStation openNewTab() {
        recheckActiveConditions();

        IncognitoNewTabPageStation page =
                IncognitoNewTabPageStation.newBuilder()
                        .withIsOpeningTabs(1)
                        .withIsSelectingTabs(1)
                        .build();

        return travelToSync(page, getNewTabButtonViewSpec()::click);
    }
}
