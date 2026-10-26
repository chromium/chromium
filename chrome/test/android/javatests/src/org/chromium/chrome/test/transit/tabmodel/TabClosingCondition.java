// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.tabmodel;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.transit.CallbackCondition;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;

import java.util.List;
import java.util.function.Supplier;

/** Fulfilled when a TabModel starts closing a tab (willCloseTab() or willCloseTabs()). */
public class TabClosingCondition extends CallbackCondition implements TabModelObserver {
    private final Supplier<TabModel> mTabModelSupplier;
    private TabModel mTabModel;

    public TabClosingCondition(Supplier<TabModel> tabModelSupplier) {
        super("willCloseTab(s)", /* numCallsExpected= */ 1);
        mTabModelSupplier = dependOnSupplier(tabModelSupplier, "TabModel");
    }

    @Override
    public void willCloseTab(Tab tab, boolean didCloseAlone) {
        notifyCalled();
    }

    @Override
    public void willCloseTabs(List<Tab> tabs, boolean isAllTabs, boolean allowUndo) {
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
        ThreadUtils.runOnUiThreadBlocking(() -> mTabModel.removeObserver(this));
    }
}
