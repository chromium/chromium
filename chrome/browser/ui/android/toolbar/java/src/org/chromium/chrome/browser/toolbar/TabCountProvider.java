// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;

import java.util.List;

/** A provider that notifies its observers when the number of tabs changes. */
public class TabCountProvider {
    /** An observer that is notified of changes to the number of open tabs. */
    public interface TabCountObserver {
        /**
         * @param tabCount Number of open tabs in the selected tab model.
         * @param isIncognito Whether the selected tab model is incognito.
         */
        void onTabCountChanged(int tabCount, boolean isIncognito);
    }

    /** List of {@link TabCountObserver}s. These are used to broadcast events to listeners. */
    private final ObserverList<TabCountObserver> mTabCountObservers;

    /** The {@link TabModelSelector} this class will observe. */
    private TabModelSelector mTabModelSelector;

    /** The {@link TabModelSelectorObserver} that observes when the tab count may have changed. */
    private TabModelSelectorObserver mTabModelSelectorObserver;

    /** The {@link TabModelObserver} that observes when the tab count may have changed. */
    private TabModelObserver mTabModelFilterObserver;

    private int mTabCount;

    private boolean mIsIncognito;

    TabCountProvider() {
        mTabCountObservers = new ObserverList<>();
    }

    /** Gets the current count of tabs. */
    public int getTabCount() {
        return mTabCount;
    }

    /**
     * @param observer The observer to add.
     */
    public void addObserver(TabCountObserver observer) {
        mTabCountObservers.addObserver(observer);
    }

    /**
     * Adds an observer and triggers the {@link TabCountObserver#onTabCountChanged(int, boolean)}
     * for the added observer.
     * @param observer The observer to add.
     */
    public void addObserverAndTrigger(TabCountObserver observer) {
        addObserver(observer);

        if (mTabModelSelector != null && mTabModelSelector.isTabStateInitialized()) {
            observer.onTabCountChanged(
                    getCurrentTotalTabCount(), mTabModelSelector.isIncognitoSelected());
        }
    }

    /**
     * @param observer The observer to remove.
     */
    public void removeObserver(TabCountObserver observer) {
        mTabCountObservers.removeObserver(observer);
    }

    /**
     * @param tabModelSelector The {@link TabModelSelectorObserver} that observes when the tab count
     *                         may have changed.
     */
    void setTabModelSelector(TabModelSelector tabModelSelector) {
        mTabModelSelector = tabModelSelector;

        mTabModelSelectorObserver = new TabModelSelectorObserver() {
            @Override
            public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                updateTabCount();
            }

            @Override
            public void onTabStateInitialized() {
                updateTabCount();
            }
        };
        mTabModelSelector.addObserver(mTabModelSelectorObserver);

        mTabModelFilterObserver = new TabModelObserver() {
            @Override
            public void didAddTab(
                    Tab tab, @TabLaunchType int type, @TabCreationState int creationState) {
                updateTabCount();
            }

            @Override
            public void tabClosureUndone(Tab tab) {
                updateTabCount();
            }

            @Override
            public void onFinishingTabClosure(Tab tab) {
                updateTabCount();
            }

            @Override
            public void tabPendingClosure(Tab tab) {
                updateTabCount();
            }

            @Override
            public void multipleTabsPendingClosure(List<Tab> tabs, boolean isAllTabs) {
                updateTabCount();
            }

            @Override
            public void tabRemoved(Tab tab) {
                updateTabCount();
            }

            @Override
            public void restoreCompleted() {
                updateTabCount();
            }
        };

        mTabModelSelector.getTabModelFilterProvider().addTabModelFilterObserver(
                mTabModelFilterObserver);

        updateTabCount();
    }

    /**
     * Clean up any state when the TabCountProvider is destroyed.
     */
    void destroy() {
        if (mTabModelFilterObserver != null) {
            mTabModelSelector.getTabModelFilterProvider().removeTabModelFilterObserver(
                    mTabModelFilterObserver);
        }

        if (mTabModelSelector != null) {
            mTabModelSelector.removeObserver(mTabModelSelectorObserver);
            mTabModelSelector = null;
        }
        mTabCountObservers.clear();
    }

    private void updateTabCount() {
        if (!mTabModelSelector.isTabStateInitialized()) return;

        final int tabCount = getCurrentTotalTabCount();
        final boolean isIncognito = mTabModelSelector.isIncognitoSelected();

        if (mTabCount == tabCount && mIsIncognito == isIncognito) return;

        mTabCount = tabCount;
        mIsIncognito = isIncognito;

        for (TabCountObserver observer : mTabCountObservers) {
            observer.onTabCountChanged(tabCount, isIncognito);
        }
    }

    private int getCurrentTotalTabCount() {
        return mTabModelSelector.getTabModelFilterProvider()
                .getCurrentTabModelFilter()
                .getTotalTabCount();
    }
}
