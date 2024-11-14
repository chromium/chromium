// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

import org.chromium.base.Token;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.test.util.tabmodel.TabBin;
import org.chromium.chrome.test.util.tabmodel.TabBinList;
import org.chromium.chrome.test.util.tabmodel.TabBinList.TabBinPosition;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.Set;

/** Utility class for binning tabs into groups for testing purposes. */
public class TabBinningUtil {

    /** Places tabs into bins representing how they are grouped in the Tab Switcher. */
    public static TabBinList binTabsByCard(TabModel tabModel) {
        List<TabBin> tabBinList = createListOfTabBins(tabModel);
        Map<Integer, TabBinPosition> tabToPositionMap = createTabToPositionMap(tabBinList);
        return new TabBinList(tabBinList, tabToPositionMap);
    }

    /**
     * Places tabs into bins representing how they are grouped in the Tab Switcher. This method
     * assumes that there are no duplicate group IDs for non-consecutive tabs.
     */
    private static List<TabBin> createListOfTabBins(TabModel tabModel) {
        List<TabBin> tabBins = new ArrayList<>();
        Set<Token> alreadySeenGroupIds = new HashSet<>();

        int binIndex = -1;
        Token prevGroupId = null;
        for (int tabIndex = 0; tabIndex < tabModel.getCount(); tabIndex++) {
            Tab tab = tabModel.getTabAt(tabIndex);
            Token groupId = tab.getTabGroupId();
            if (groupId == null || !Objects.equals(prevGroupId, groupId)) {
                binIndex++;
                verifyNoDuplicateGroupIds(groupId, alreadySeenGroupIds);

                // Add new Bin to list.
                int rootId = tab.getRootId();
                tabBins.add(new TabBin(groupId, rootId));
            }
            List<Tab> tabList = tabBins.get(binIndex).tabs;
            tabList.add(tab);
            prevGroupId = groupId;
        }
        return tabBins;
    }

    /**
     * Verifies that the current tab group's group ID is not a duplicate.
     *
     * @param groupId The current tab group's group ID.
     * @param alreadySeenGroupIds the set of group IDs that have already been seen.
     */
    private static void verifyNoDuplicateGroupIds(Token groupId, Set<Token> alreadySeenGroupIds) {
        if (groupId != null) {
            assert !alreadySeenGroupIds.contains(groupId);
        }
        alreadySeenGroupIds.add(groupId);
    }

    /** Creates an ordered map which maps tab IDs to their position in the tab switcher. */
    private static Map<Integer, TabBinPosition> createTabToPositionMap(List<TabBin> tabBins) {
        Map<Integer, TabBinPosition> map = new LinkedHashMap<>();
        int binIndex = 0;
        for (TabBin bin : tabBins) {
            int tabIndex = 0;
            for (Tab tab : bin.tabs) {
                TabBinPosition position = new TabBinPosition(binIndex, tabIndex);
                map.put(tab.getId(), position);
                tabIndex++;
            }
            binIndex++;
        }
        return map;
    }
}
