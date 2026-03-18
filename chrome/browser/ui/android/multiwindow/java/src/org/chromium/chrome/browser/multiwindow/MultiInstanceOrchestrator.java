// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab.Tab;

import java.util.List;

/**
 * Class that orchestrates and provides for common tasks applicable to all windows in a multi-window
 * environment.
 */
@NullMarked
public interface MultiInstanceOrchestrator {
    /**
     * Moves the specified tabs to the specified ChromeTabbedActivity instance. This accepts inputs
     * to determine the position of the moved tabs in the destination window. The operation will
     * fail if the instance is not found.
     *
     * @param destWindowId The id of the destination window.
     * @param tabs The list of tabs to move.
     * @param destTabIndex The tab index in the destination window where the tabs will be
     *     positioned. This will be ignored if {@code destGroupTabId} is set. To use the default tab
     *     index, set this to {@code TabList.INVALID_TAB_INDEX}.
     * @param destGroupTabId The id of the tab in the destination tab group, if the tabs need to be
     *     moved to a specific tab group in the destination window. The tabs will be added to the
     *     end of the destination tab group. A tab with this id must exist in the destination
     *     window, otherwise this operation will fail. If there is no tab group to move the
     *     specified tabs to, set this to {@code TabList.INVALID_TAB_INDEX}.
     */
    void moveTabsToWindowByIdChecked(
            int destWindowId, List<Tab> tabs, int destTabIndex, int destGroupTabId);
}
