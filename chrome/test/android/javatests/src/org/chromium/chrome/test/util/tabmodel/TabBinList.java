// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.tabmodel;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;

/** Data class containing information on how tabs are grouped in the Tab Switcher. */
public class TabBinList {
    public final List<TabBin> tabBinList;
    public final Map<Integer, TabBinPosition> tabIdToPositionMap;

    /**
     * Constructor. Contains two data structures used to represent cards in the tab group switcher.
     *
     * @param tabBinList represents how tabs are grouped in the Tab Switcher, separated by card.
     * @param tabIdToPositionMap is a map for mapping a tab ID to its {@link TabBinPosition}.
     */
    public TabBinList(List<TabBin> tabBinList, Map<Integer, TabBinPosition> tabIdToPositionMap) {
        this.tabBinList = tabBinList;
        this.tabIdToPositionMap = tabIdToPositionMap;
    }

    /** Used to represent a Tab's position in the tab switcher. */
    public static class TabBinPosition {
        public final int cardIndexInTabSwitcher;
        public final int tabIndexInGroup;

        /**
         * A constructor. Accepts two arguments representing the position of a Tab in the tab
         * switcher.
         *
         * @param cardIndexInTabSwitcher The index of the card (in the Tab Switcher) the tab is
         *     located in.
         * @param tabIndexInGroup The position of the tab in the tab group.
         */
        public TabBinPosition(int cardIndexInTabSwitcher, int tabIndexInGroup) {
            this.cardIndexInTabSwitcher = cardIndexInTabSwitcher;
            this.tabIndexInGroup = tabIndexInGroup;
        }
    }

    /** Returns a representation of the bins like "{[11, 12], 13, [14], [15, 16, 17]}". */
    public String getTabIdsAsString() {
        List<String> tabBinListStrings = new ArrayList<>();
        for (TabBin bin : tabBinList) {
            tabBinListStrings.add(bin.getTabIdsAsString());
        }
        return "{" + String.join(", ", tabBinListStrings) + "}";
    }
}
