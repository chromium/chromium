// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import org.chromium.base.test.transit.CallbackCondition;
import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * The screen that shows a webpage with the omnibox and the toolbar.
 *
 * <p>Contains extra Conditions compared ot the BasePageStation that can be checked when not at an
 * entry point: ensure the TabModel adds and selects the new Tab.
 */
public class PageStation extends BasePageStation {
    private final boolean mIsOpeningTab;
    protected PageStationTabModelObserver mTabModelObserver;
    protected PageLoadedCondition mPageLoadedEnterCondition;

    public PageStation(
            ChromeTabbedActivityTestRule chromeTabbedActivityTestRule,
            boolean incognito,
            boolean isOpeningTab) {
        super(chromeTabbedActivityTestRule, incognito);
        mIsOpeningTab = isOpeningTab;
    }

    @Override
    protected void onStartMonitoringTransitionTo() {
        mTabModelObserver = new PageStationTabModelObserver();
        mTabModelObserver.install();
    }

    @Override
    protected void onStopMonitoringTransitionTo() {
        mTabModelObserver.uninstall();
        mTabModelObserver = null;
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        super.declareElements(elements);

        if (mIsOpeningTab) {
            elements.declareEnterCondition(
                    CallbackCondition.instrumentationThread(
                            mTabModelObserver.mTabAddedCallback, "Receive tab opened callback"));
        }
        elements.declareEnterCondition(
                CallbackCondition.instrumentationThread(
                        mTabModelObserver.mTabSelectedCallback, "Receive tab selected callback"));

        mPageLoadedEnterCondition =
                new PageLoadedCondition(mChromeTabbedActivityTestRule, mIncognito);
        elements.declareEnterCondition(mPageLoadedEnterCondition);
        elements.declareEnterCondition(new PageInteractableCondition(mPageLoadedEnterCondition));
    }

    private class PageStationTabModelObserver implements TabModelObserver {
        private CallbackHelper mTabAddedCallback = new CallbackHelper();
        private CallbackHelper mTabSelectedCallback = new CallbackHelper();

        @Override
        public void didSelectTab(Tab tab, int type, int lastId) {
            mTabSelectedCallback.notifyCalled();
        }

        @Override
        public void didAddTab(Tab tab, int type, int creationState, boolean markedForSelection) {
            mTabAddedCallback.notifyCalled();
        }

        public void install() {
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        mChromeTabbedActivityTestRule
                                .getActivity()
                                .getTabModelSelector()
                                .getModel(mIncognito)
                                .addObserver(mTabModelObserver);
                    });
        }

        private void uninstall() {
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        mChromeTabbedActivityTestRule
                                .getActivity()
                                .getTabModelSelector()
                                .getModel(mIncognito)
                                .removeObserver(mTabModelObserver);
                    });
        }
    }
}
