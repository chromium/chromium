// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.page;

import android.text.TextUtils;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.transit.CallbackCondition;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;

/**
 * Fulfilled when the Tab calls onLoadStopped() for the expected URL (or any URL if null) after
 * monitoring starts, so that the state from before a load trigger isn't mistaken for the new load.
 * Unlike {@link PageLoadCallbackCondition}, this also covers same-document loads, failed loads and
 * pages restored from the back/forward cache, which don't call onPageLoadFinished().
 */
@NullMarked
public class PageLoadStoppedCondition extends CallbackCondition implements TabObserver {
    private final Tab mTab;
    private final @Nullable String mExpectedUrl;

    public PageLoadStoppedCondition(Tab tab, @Nullable String expectedUrl) {
        super(
                expectedUrl == null ? "onLoadStopped" : "onLoadStopped for " + expectedUrl,
                /* numCallsExpected= */ 1);
        mTab = tab;
        mExpectedUrl = expectedUrl;
    }

    @Override
    public void onLoadStopped(Tab tab, boolean toDifferentDocument) {
        if (mExpectedUrl == null || TextUtils.equals(tab.getUrl().getSpec(), mExpectedUrl)) {
            notifyCalled();
        }
    }

    @Override
    public void onStartMonitoring() {
        super.onStartMonitoring();
        ThreadUtils.runOnUiThreadBlocking(() -> mTab.addObserver(this));
    }

    @Override
    public void onStopMonitoring() {
        super.onStopMonitoring();
        ThreadUtils.runOnUiThreadBlocking(() -> mTab.removeObserver(this));
    }
}
