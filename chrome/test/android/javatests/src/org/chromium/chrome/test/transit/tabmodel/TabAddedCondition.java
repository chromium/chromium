// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.tabmodel;

import androidx.annotation.Nullable;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.transit.CallbackCondition;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;

import java.util.function.Supplier;

// TODO(crbug.com/576318784): CallbackCondition ignores the expected number of calls when the
// starting count is 0, so this is fulfilled after the first tab even if more are expected.
/**
 * Fulfilled when the expected number of tabs are added to a TabModel. Supplies the last added Tab.
 */
public class TabAddedCondition extends CallbackCondition
        implements TabModelObserver, Supplier<Tab> {
    private TabModel mTabModel;
    private @Nullable Tab mLastAddedTab;
    private final Supplier<TabModel> mTabModelSupplier;

    public TabAddedCondition(int numTabsBeingOpened, Supplier<TabModel> tabModelSupplier) {
        super("didAddTab", numTabsBeingOpened);
        mTabModelSupplier = dependOnSupplier(tabModelSupplier, "TabModel");
    }

    @Override
    public void didAddTab(
            Tab tab,
            @TabLaunchType int type,
            @TabCreationState int creationState,
            boolean markedForSelection) {
        mLastAddedTab = tab;
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
    public @Nullable Tab get() {
        return mLastAddedTab;
    }
}
