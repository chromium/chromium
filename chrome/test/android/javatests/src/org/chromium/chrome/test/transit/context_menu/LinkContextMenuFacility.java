// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.context_menu;

import static androidx.test.espresso.matcher.ViewMatchers.withText;

import org.chromium.chrome.R;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.transit.tabmodel.TabCountChangedCondition;
import org.chromium.chrome.test.transit.tabmodel.TabGroupUiFacility;

/**
 * Facility represents a context menu triggered for a text link. This has to be used for a webpage.
 */
public class LinkContextMenuFacility extends ContextMenuFacility {
    private Item mOpenTabInNewTab;
    private Item mOpenTabInNewTabInGroup;

    @Override
    protected void declareItems(ItemsBuilder items) {
        super.declareItems(items);

        mOpenTabInNewTab =
                items.declareItem(
                        itemViewSpec(withText(R.string.contextmenu_open_in_new_tab)), null);

        mOpenTabInNewTabInGroup =
                items.declareItem(
                        itemViewSpec(withText(R.string.contextmenu_open_in_new_tab_group)), null);
    }

    /** Click the "Open in new tab" item in the context menu. */
    public void openInNewTab() {
        assert mHostStation != null;
        assert mOpenTabInNewTab != null;
        mOpenTabInNewTab
                .scrollToAndSelectTo()
                .waitFor(new TabCountChangedCondition(mHostStation.getTabModel(), 1));
    }

    /** Click the "Open in new tab in group" item in the context menu. */
    public TabGroupUiFacility<WebPageStation> openTabInNewGroup() {
        assert mHostStation != null;
        assert mOpenTabInNewTabInGroup != null;
        return mOpenTabInNewTabInGroup
                .scrollToAndSelectTo()
                .enterFacility(new TabGroupUiFacility<>());
    }
}
