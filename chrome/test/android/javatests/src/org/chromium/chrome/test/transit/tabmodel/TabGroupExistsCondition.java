// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.tabmodel;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.transit.ConditionStatus;
import org.chromium.base.test.transit.UiThreadCondition;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** Check that the tab group represented by the card exists in the tab model. */
public class TabGroupExistsCondition extends UiThreadCondition {

    private final Supplier<TabModelSelector> mTabModelSelectorSupplier;
    private final boolean mIncognito;
    private final List<Integer> mTabIdsToGroup;

    public TabGroupExistsCondition(
            boolean incognito,
            List<Integer> tabIdsToGroup,
            Supplier<TabModelSelector> tabModelSelectorSupplier) {
        mTabModelSelectorSupplier = dependOnSupplier(tabModelSelectorSupplier, "TabModelSelector");
        mIncognito = incognito;
        mTabIdsToGroup = tabIdsToGroup;
    }

    @Override
    protected ConditionStatus checkWithSuppliers() {
        TabGroupModelFilter groupFilter =
                mTabModelSelectorSupplier
                        .get()
                        .getTabGroupModelFilterProvider()
                        .getTabGroupModelFilter(mIncognito);
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
        return String.format(
                "%s TabGroup exists with tabIds %s",
                mIncognito ? "Incognito" : "Regular", mTabIdsToGroup);
    }
}
