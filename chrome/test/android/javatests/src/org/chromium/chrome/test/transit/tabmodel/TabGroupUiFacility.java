// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.tabmodel;

import static androidx.test.espresso.matcher.ViewMatchers.withId;

import android.view.View;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.transit.page.PageStation;

import java.util.List;

/**
 * Represents the bottom tab strip UI. As TabStrip UI can show on both NTP and web pages.
 *
 * @param <HostStationT> Page that can be a simple {@link PageStation}, or a {@link WebPageStation}.
 */
public class TabGroupUiFacility<HostStationT extends PageStation> extends Facility<HostStationT> {
    private final Supplier<TabModelSelector> mTabModelSelectorSupplier;
    private final List<Integer> mTabIds;
    public ViewElement<View> viewElement;

    /** Create facility with expected tab Ids in the group. */
    public TabGroupUiFacility(
            Supplier<TabModelSelector> tabModelSelectorSupplier, List<Integer> tabIds) {
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mTabIds = tabIds;

        assert mTabIds.size() >= 1 : "Expect at least one tabId.";
    }

    /** Create facility with unknown tab ids. */
    public TabGroupUiFacility(Supplier<TabModelSelector> tabModelSelectorSupplier) {
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mTabIds = List.of();
    }

    @Override
    public void declareExtraElements() {
        // Ensure the tab group UI is visible.
        viewElement = declareView(withId(R.id.tab_group_ui_toolbar_view));

        if (!mTabIds.isEmpty()) {
            // Ensure the number of tabs are in group.
            declareEnterCondition(
                    new TabGroupExistsCondition(
                            mHostStation.isIncognito(), mTabIds, mTabModelSelectorSupplier));
        }
    }
}
