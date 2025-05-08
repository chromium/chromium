// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.context_menu;

import static androidx.test.espresso.matcher.ViewMatchers.withText;

import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.Transition;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.transit.tabmodel.TabCountChangedCondition;
import org.chromium.chrome.test.transit.tabmodel.TabGroupUiFacility;

import java.util.List;

/**
 * Facility represents a context menu triggered for a text link. This has to be used for a webpage.
 */
public class LinkContextMenuFacility extends ContextMenuFacility {
    private Item<Void> mOpenTabInNewTab;
    private Item<TabGroupUiFacility<WebPageStation>> mOpenTabInNewTabInGroup;

    @Override
    protected void declareItems(ItemsBuilder items) {
        super.declareItems(items);

        mOpenTabInNewTab =
                items.declareItem(
                        itemViewSpec(withText(R.string.contextmenu_open_in_new_tab)),
                        null,
                        this::createTabInBackground);

        mOpenTabInNewTabInGroup =
                items.declareItem(
                        itemViewSpec(withText(R.string.contextmenu_open_in_new_tab_group)),
                        null,
                        this::createTabInBackgroundInGroup);
    }

    /** Click the "Open in new tab" item in the context menu. */
    public Void openInNewTab() {
        assert mOpenTabInNewTab != null;
        return mOpenTabInNewTab.scrollToAndSelect();
    }

    /** Click the "Open in new tab in group" item in the context menu. */
    public TabGroupUiFacility<WebPageStation> openTabInNewGroup() {
        assert mOpenTabInNewTabInGroup != null;
        return mOpenTabInNewTabInGroup.scrollToAndSelect();
    }

    private Void createTabInBackground(ItemOnScreenFacility<Void> itemOnScreen) {
        assert mHostStation != null;
        TabModel tabModel =
                mHostStation.getActivity().getTabModelSelectorSupplier().get().getCurrentModel();
        Condition tabCountIncrease = new TabCountChangedCondition(tabModel, 1);
        mHostStation.exitFacilitiesSync(
                List.of(this, itemOnScreen),
                Transition.conditionOption(tabCountIncrease),
                itemOnScreen.viewElement.getClickTrigger());
        return null;
    }

    private TabGroupUiFacility<WebPageStation> createTabInBackgroundInGroup(
            ItemOnScreenFacility<TabGroupUiFacility<WebPageStation>> itemOnScreen) {
        assert mHostStation != null;
        return mHostStation.swapFacilitySync(
                this,
                new TabGroupUiFacility<>(mHostStation.getActivity().getTabModelSelectorSupplier()),
                itemOnScreen.viewElement.getClickTrigger());
    }
}
