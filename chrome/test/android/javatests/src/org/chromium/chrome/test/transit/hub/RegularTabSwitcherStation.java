// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.hub;

import static androidx.test.espresso.matcher.ViewMatchers.isSelected;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.chromium.base.test.transit.ViewElement.unscopedOption;
import static org.chromium.chrome.test.util.ChromeTabUtils.getTabCountOnUiThread;

import android.view.View;

import androidx.recyclerview.widget.RecyclerView;

import org.hamcrest.Matcher;

import org.chromium.base.test.transit.ViewElementMatchesCondition;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;

/** Regular tab switcher pane station. */
public class RegularTabSwitcherStation extends TabSwitcherStation {
    public static final Matcher<View> EMPTY_STATE_TEXT =
            withText(R.string.tabswitcher_no_tabs_empty_state);

    public RegularTabSwitcherStation(boolean regularTabsExist, boolean incognitoTabsExist) {
        super(/* isIncognito= */ false, regularTabsExist, incognitoTabsExist);

        assert regularTabsButtonElement != null;
        declareEnterCondition(
                new ViewElementMatchesCondition(regularTabsButtonElement, isSelected()));
        if (mRegularTabsExist) {
            recyclerViewElement =
                    declareView(
                            paneHostElement.descendant(
                                    RecyclerView.class, withId(R.id.tab_list_recycler_view)),
                            unscopedOption());
        } else {
            declareView(EMPTY_STATE_TEXT);
            recyclerViewElement = null;
        }
    }

    /**
     * Build a {@link RegularTabSwitcherStation} assuming the tab model doesn't change in the
     * transition.
     */
    public static RegularTabSwitcherStation from(TabModelSelector selector) {
        return new RegularTabSwitcherStation(
                getTabCountOnUiThread(selector.getModel(false)) > 0,
                getTabCountOnUiThread(selector.getModel(true)) > 0);
    }

    @Override
    public @PaneId int getPaneId() {
        return PaneId.TAB_SWITCHER;
    }

    /** Open a new tab using the New Tab action button. */
    public RegularNewTabPageStation openNewTab() {
        recheckActiveConditions();

        return newTabButtonElement
                .clickTo()
                .arriveAt(RegularNewTabPageStation.newBuilder().initOpeningNewTab().build());
    }

    public ArchiveMessageCardFacility expectArchiveMessageCard() {
        return noopTo().enterFacility(
                        new ArchiveMessageCardFacility(/* tabSwitcherStation= */ this));
    }
}
