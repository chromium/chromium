// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.hub;

import static androidx.test.espresso.matcher.ViewMatchers.isSelected;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.chromium.base.test.transit.ViewElement.unscopedOption;
import static org.chromium.chrome.test.util.ChromeTabUtils.getTabCountOnUiThread;

import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.test.transit.ViewElementMatchesCondition;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.transit.ntp.IncognitoNewTabPageStation;

/** Incognito tab switcher pane station. */
public class IncognitoTabSwitcherStation extends TabSwitcherStation {

    public IncognitoTabSwitcherStation(boolean regularTabsExist, boolean incognitoTabsExist) {
        super(/* isIncognito= */ true, regularTabsExist, incognitoTabsExist);

        if (!mIsStandaloneIncognitoWindow) {
            assert incognitoTabsButtonElement != null;
            declareEnterCondition(
                    new ViewElementMatchesCondition(incognitoTabsButtonElement, isSelected()));
        }

        recyclerViewElement =
                declareView(
                        paneHostElement.descendant(
                                RecyclerView.class,
                                withId(org.chromium.chrome.test.R.id.tab_list_recycler_view)),
                        unscopedOption());
    }

    /**
     * Build an {@link IncognitoTabSwitcherStation} assuming the tab model doesn't change in the
     * transition.
     */
    public static IncognitoTabSwitcherStation from(TabModelSelector selector) {
        return new IncognitoTabSwitcherStation(
                getTabCountOnUiThread(selector.getModel(false)) > 0,
                getTabCountOnUiThread(selector.getModel(true)) > 0);
    }

    @Override
    public @PaneId int getPaneId() {
        return PaneId.INCOGNITO_TAB_SWITCHER;
    }

    /** Open a new tab using the New Tab action button. */
    public IncognitoNewTabPageStation openNewTab() {
        recheckActiveConditions();

        return newTabButtonElement
                .clickTo()
                .arriveAt(IncognitoNewTabPageStation.newBuilder().initOpeningNewTab().build());
    }
}
