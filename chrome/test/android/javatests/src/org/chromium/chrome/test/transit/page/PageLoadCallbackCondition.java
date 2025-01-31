// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.page;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.transit.ConditionStatus;
import org.chromium.base.test.transit.UiThreadCondition;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/** Waits for the page load started and finished callbacks. */
public class PageLoadCallbackCondition extends UiThreadCondition {
    private final Tab mLoadedTab;
    private final List<String> mCallbacks = new ArrayList<>();
    private final TabLoadEventLogger mTabEventLogger = new TabLoadEventLogger();

    public PageLoadCallbackCondition(Tab loadedTab) {
        mLoadedTab = loadedTab;
    }

    @Override
    public void onStartMonitoring() {
        super.onStartMonitoring();
        ThreadUtils.runOnUiThreadBlocking(() -> mLoadedTab.addObserver(mTabEventLogger));
    }

    @Override
    public void onStopMonitoring() {
        super.onStopMonitoring();
        ThreadUtils.runOnUiThreadBlocking(() -> mLoadedTab.removeObserver(mTabEventLogger));
    }

    @Override
    protected ConditionStatus checkWithSuppliers() throws Exception {
        String status = String.join(", ", mCallbacks);
        return whether(
                mCallbacks.contains("onPageLoadStarted")
                        && mCallbacks.contains("onPageLoadFinished"),
                status);
    }

    @Override
    public String buildDescription() {
        return "onPageLoadStarted() and onPageLoadFinished() received";
    }

    private class TabLoadEventLogger extends EmptyTabObserver {
        @Override
        public void onInitialized(Tab tab, String appId) {
            mCallbacks.add("onInitialized");
        }

        @Override
        public void onShown(Tab tab, int type) {
            mCallbacks.add("onShown");
        }

        @Override
        public void onHidden(Tab tab, int type) {
            mCallbacks.add("onHidden");
        }

        @Override
        public void onClosingStateChanged(Tab tab, boolean closing) {
            mCallbacks.add("onClosingStateChanged");
        }

        @Override
        public void onDestroyed(Tab tab) {
            mCallbacks.add("onDestroyed");
        }

        @Override
        public void onLoadUrl(Tab tab, LoadUrlParams params, Tab.LoadUrlResult loadUrlResult) {
            mCallbacks.add("onLoadUrl");
        }

        @Override
        public void onPageLoadStarted(Tab tab, GURL url) {
            mCallbacks.add("onPageLoadStarted");
        }

        @Override
        public void onPageLoadFinished(Tab tab, GURL url) {
            mCallbacks.add("onPageLoadFinished");
        }

        @Override
        public void onPageLoadFailed(Tab tab, int errorCode) {
            mCallbacks.add("onPageLoadFailed");
        }

        @Override
        public void onLoadStarted(Tab tab, boolean toDifferentDocument) {
            mCallbacks.add("onLoadStarted");
        }

        @Override
        public void onLoadStopped(Tab tab, boolean toDifferentDocument) {
            mCallbacks.add("onLoadStopped");
        }
    }
}
