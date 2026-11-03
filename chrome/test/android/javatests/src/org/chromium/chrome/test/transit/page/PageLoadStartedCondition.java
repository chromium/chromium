// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.page;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.transit.CallbackCondition;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.url.GURL;

import java.util.function.Supplier;

/** Fulfilled when the Tab calls onPageLoadStarted() after monitoring starts. */
@NullMarked
public class PageLoadStartedCondition extends CallbackCondition implements TabObserver {
    private final Supplier<Tab> mLoadedTabSupplier;
    private @Nullable Tab mTab;

    public PageLoadStartedCondition(Supplier<Tab> loadedTabSupplier) {
        super("onPageLoadStarted", /* numCallsExpected= */ 1);
        mLoadedTabSupplier = dependOnSupplier(loadedTabSupplier, "LoadedTab");
    }

    @Override
    public void onPageLoadStarted(Tab tab, GURL url) {
        notifyCalled();
    }

    @Override
    public void onStartMonitoring() {
        super.onStartMonitoring();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Tab tab = mLoadedTabSupplier.get();
                    tab.addObserver(this);
                    mTab = tab;
                });
    }

    @Override
    public void onStopMonitoring() {
        super.onStopMonitoring();
        Tab tab = assumeNonNull(mTab);
        ThreadUtils.runOnUiThreadBlocking(() -> tab.removeObserver(this));
    }
}
