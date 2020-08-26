// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser.tabmodel;

import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.EmptyTabModel;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;

import java.util.ArrayList;

/**
 * Almost empty implementation to mock a TabModel. It only handles tab creation and queries.
 */
public class MockTabModel extends EmptyTabModel {
    /**
     * Used to create different kinds of Tabs.  If a MockTabModelDelegate is not provided, regular
     * Tabs are produced.
     */
    public interface MockTabModelDelegate  {
        /**
         * Creates a Tab.
         * @param id ID of the Tab.
         * @param incognito Whether the Tab is incognito.
         * @return Tab that is created.
         */
        public Tab createTab(int id, boolean incognito);
    }

    private int mIndex = TabModel.INVALID_TAB_INDEX;

    private final ObserverList<TabModelObserver> mObservers = new ObserverList<>();
    private final ArrayList<Tab> mTabs = new ArrayList<Tab>();
    private final boolean mIncognito;
    private final MockTabModelDelegate mDelegate;

    public MockTabModel(boolean incognito, MockTabModelDelegate delegate) {
        mIncognito = incognito;
        mDelegate = delegate;
    }

    public Tab addTab(int id) {
        Tab tab = mDelegate == null ? new MockTab(id, isIncognito())
                                    : mDelegate.createTab(id, isIncognito());
        mTabs.add(tab);
        return tab;
    }

    @Override
    public void addTab(
            Tab tab, int index, @TabLaunchType int type, @TabCreationState int creationState) {
        if (index == -1) {
            mTabs.add(tab);
        } else {
            mTabs.add(index, tab);
            if (index <= mIndex) {
                mIndex++;
            }
        }

        for (TabModelObserver observer : mObservers) observer.didAddTab(tab, type, creationState);
    }

    @Override
    public void removeTab(Tab tab) {
        if (mTabs.remove(tab)) {
            for (TabModelObserver observer : mObservers) observer.tabRemoved(tab);
        }
    }

    @Override
    public boolean isIncognito() {
        return mIncognito;
    }

    @Override
    public int getCount() {
        return mTabs.size();
    }

    @Override
    public Tab getTabAt(int position) {
        return mTabs.get(position);
    }

    @Override
    public int indexOf(Tab tab) {
        return mTabs.indexOf(tab);
    }

    @Override
    public int index() {
        return mIndex;
    }

    @Override
    public void setIndex(int i, @TabSelectionType int type) {
        mIndex = i;
    }

    @Override
    public void addObserver(TabModelObserver observer) {
        mObservers.addObserver(observer);
    }

    @Override
    public void removeObserver(TabModelObserver observer) {
        mObservers.removeObserver(observer);
    }
}
