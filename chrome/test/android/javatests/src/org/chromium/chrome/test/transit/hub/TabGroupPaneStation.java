// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.hub;

import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.junit.Assert.assertNotNull;

import android.view.View;

import androidx.annotation.Nullable;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.Journeys;

/** The tab groups pane station. */
public class TabGroupPaneStation extends HubBaseStation {
    public ViewElement<RecyclerView> recyclerViewElement;
    public @Nullable ViewElement<View> newTabGroupButtonElement;

    public TabGroupPaneStation(boolean regularTabsExist, boolean incognitoTabsExist) {
        super(regularTabsExist, incognitoTabsExist, /* hasMenuButton= */ false);

        // TODO(crbug.com/413652567): Handle empty state case.
        recyclerViewElement =
                declareView(
                        paneHostElement.descendant(
                                RecyclerView.class, withId(R.id.tab_group_list_recycler_view)));

        if (ChromeFeatureList.sTabGroupEntryPointsAndroid.isEnabled()) {
            newTabGroupButtonElement =
                    declareView(toolbarElement.descendant(withId(R.id.toolbar_action_button)));
        }
    }

    @Override
    public @PaneId int getPaneId() {
        return PaneId.TAB_GROUPS;
    }

    public NewTabGroupDialogFacility<TabGroupPaneStation> createNewTabGroup() {
        assertNotNull(newTabGroupButtonElement);
        return Journeys.beginNewTabGroupUiFlow(this, newTabGroupButtonElement.getClickTrigger());
    }

    // TODO(crbug.com/413652567): Implement actions.
}
