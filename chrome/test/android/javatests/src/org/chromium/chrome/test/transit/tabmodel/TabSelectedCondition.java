// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.tabmodel;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.transit.CallbackCondition;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Supplier;

/**
 * Fulfilled when the expected number of different tabs are selected in a TabModel. Supplies the
 * last selected Tab.
 */
public class TabSelectedCondition extends CallbackCondition
        implements TabModelObserver, Supplier<Tab> {
    private final List<Tab> mTabsSelected = new ArrayList<>();
    private final Supplier<TabModel> mTabModelSupplier;
    private TabModel mTabModel;

    public TabSelectedCondition(int numTabsBeingSelected, Supplier<TabModel> tabModelSupplier) {
        super("didSelectTab", numTabsBeingSelected);
        mTabModelSupplier = dependOnSupplier(tabModelSupplier, "TabModel");
    }

    @Override
    public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
        if (mTabsSelected.contains(tab)) {
            // We get multiple (2-3 depending on the case) didSelectTab when selecting a Tab, so
            // filter out redundant callbacks to make sure we wait for different Tabs.
            return;
        }
        mTabsSelected.add(tab);
        notifyCalled();
    }

    @Override
    public void onStartMonitoring() {
        super.onStartMonitoring();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabModel = mTabModelSupplier.get();
                    mTabModel.addObserver(this);
                });
    }

    @Override
    public void onStopMonitoring() {
        super.onStopMonitoring();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabModel.removeObserver(this);
                });
    }

    @Override
    public Tab get() {
        if (mTabsSelected.isEmpty()) {
            return null;
        }
        return mTabsSelected.get(mTabsSelected.size() - 1);
    }
}
