// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.tab_restore.HistoricalEntry;
import org.chromium.chrome.browser.tab.tab_restore.HistoricalTabSaver;
import org.chromium.chrome.browser.tab.tab_restore.HistoricalTabSaverImpl;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.List;

/**
 * Test support for reading/writing entries from/to TabRestoreService.
 */
public class TabRestoreServiceUtils {
    public static final int MAX_ENTRY_COUNT = 5;

    /**
     * Clears all TabRestoreService entries.
     */
    public static void clearEntries(TabModelSelector tabModelSelector) {

    }

    /**
     * Creates a single Tab entry for a {@link Tab}.
     */
    public static void createTabEntry(TabModel tabModel, Tab tab) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            final HistoricalTabSaver saver = new HistoricalTabSaverImpl(tabModel);
            saver.createHistoricalTab(tab);
        });
    }

    /**
     * Creates a single Tab or Group entry for a {@link HistoricalTabGroup}.
     */
    public static void createTabOrGroupEntry(TabModel tabModel, HistoricalEntry entry) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            final HistoricalTabSaver saver = new HistoricalTabSaverImpl(tabModel);
            saver.createHistoricalTabOrGroup(entry);
        });
    }

    /**
     * Creates a single Window entry for a list of {@link HistoricalEntry}.
     */
    public static void createWindowEntry(TabModel tabModel, List<HistoricalEntry> entries) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            final HistoricalTabSaver saver = new HistoricalTabSaverImpl(tabModel);
            saver.createHistoricalBulkClosure(entries);
        });
    }
}
