// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.tabmodel;

import org.chromium.base.test.transit.ConditionStatus;
import org.chromium.base.test.transit.UiThreadCondition;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.function.Supplier;

/** Check that the tab group represented by the card exists in the tab model. */
public class TabGroupExistsCondition extends UiThreadCondition {

    private final Supplier<TabGroupModelFilter> mTabGroupModelFilterSupplier;
    private final List<Integer> mTabIdsToGroup;

    public TabGroupExistsCondition(
            Supplier<TabGroupModelFilter> tabGroupModelFilterSupplier,
            List<Integer> tabIdsToGroup) {
        mTabGroupModelFilterSupplier =
                dependOnSupplier(tabGroupModelFilterSupplier, "TabGroupModelFilter");
        mTabIdsToGroup = tabIdsToGroup;
    }

    @Override
    protected ConditionStatus checkWithSuppliers() {
        TabGroupModelFilter groupFilter = mTabGroupModelFilterSupplier.get();
        List<Tab> relatedTabs = groupFilter.getRelatedTabList(mTabIdsToGroup.get(0));
        if (relatedTabs.isEmpty()) {
            return notFulfilled("relatedTabIds is empty");
        }

        List<@TabId Integer> tabIds = new ArrayList<>(relatedTabs.size());
        for (Tab tab : relatedTabs) {
            tabIds.add(tab.getId());
        }

        Collections.sort(tabIds);
        return whether(mTabIdsToGroup.equals(tabIds), "tabIds: %s", tabIds);
    }

    @Override
    public String buildDescription() {
        return String.format("TabGroup exists with tabIds %s", mTabIdsToGroup);
    }
}
