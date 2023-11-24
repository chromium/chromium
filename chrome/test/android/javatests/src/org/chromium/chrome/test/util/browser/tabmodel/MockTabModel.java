// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser.tabmodel;

import androidx.annotation.NonNull;

import org.chromium.base.ObserverList;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.EmptyTabModel;
import org.chromium.chrome.browser.tabmodel.IncognitoTabModel;
import org.chromium.chrome.browser.tabmodel.IncognitoTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;

import java.util.ArrayList;

/** Almost empty implementation to mock a TabModel. It only handles tab creation and queries. */
public class MockTabModel extends EmptyTabModel implements IncognitoTabModel {
    /**
     * Used to create different kinds of Tabs.  If a MockTabModelDelegate is not provided, regular
     * Tabs are produced.
     */
    public interface MockTabModelDelegate {
        /**
         * Creates a Tab.
         * @param id ID of the Tab.
         * @param incognito Whether the Tab is incognito.
         * @return Tab that is created.
         */
        public MockTab createTab(int id, boolean incognito);
    }

    private int mIndex = TabModel.INVALID_TAB_INDEX;

    private final ObservableSupplierImpl<Tab> mCurrentTabSupplier = new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<Integer> mTabCountSupplier =
            new ObservableSupplierImpl<>();
    private final ObserverList<TabModelObserver> mObservers = new ObserverList<>();
    private final ArrayList<Tab> mTabs = new ArrayList<Tab>();
    private final Profile mProfile;
    private final MockTabModelDelegate mDelegate;
    private boolean mIsActiveModel;

    public MockTabModel(Profile profile, MockTabModelDelegate delegate) {
        mProfile = profile;
        mDelegate = delegate;
        mTabCountSupplier.set(0);
    }

    public MockTab addTab(int id) {
        MockTab tab =
                mDelegate == null
                        ? new MockTab(id, mProfile)
                        : mDelegate.createTab(id, isIncognito());

        addTab(
                tab,
                TabModel.INVALID_TAB_INDEX,
                TabLaunchType.FROM_CHROME_UI,
                TabCreationState.LIVE_IN_FOREGROUND);
        return tab;
    }

    @Override
    public @NonNull ObservableSupplier<Integer> getTabCountSupplier() {
        return mTabCountSupplier;
    }

    @Override
    public void addTab(
            Tab tab, int index, @TabLaunchType int type, @TabCreationState int creationState) {
        for (TabModelObserver observer : mObservers) observer.willAddTab(tab, type);

        if (index == TabModel.INVALID_TAB_INDEX) {
            mTabs.add(tab);
        } else {
            mTabs.add(index, tab);
            if (index <= mIndex) {
                mIndex++;
            }
        }
        mTabCountSupplier.set(mTabs.size());

        for (TabModelObserver observer : mObservers) {
            observer.didAddTab(tab, type, creationState, false);
        }
    }

    @Override
    public void removeTab(Tab tab) {
        if (mTabs.remove(tab)) {
            mTabCountSupplier.set(mTabs.size());
            for (TabModelObserver observer : mObservers) observer.tabRemoved(tab);
        }
    }

    @Override
    public Profile getProfile() {
        return mProfile;
    }

    @Override
    public boolean isIncognito() {
        return mProfile.isOffTheRecord();
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
    public @NonNull ObservableSupplier<Tab> getCurrentTabSupplier() {
        return mCurrentTabSupplier;
    }

    @Override
    public int index() {
        return mIndex;
    }

    @Override
    public void setIndex(int i, @TabSelectionType int type, boolean skipLoadingTab) {
        mIndex = i;
        mCurrentTabSupplier.set(TabModelUtils.getCurrentTab(this));
        if (mIndex == TabModel.INVALID_TAB_INDEX) return;

        for (TabModelObserver observer : mObservers) {
            observer.didSelectTab(mTabs.get(mIndex), type, mIndex);
        }
    }

    @Override
    public void addObserver(TabModelObserver observer) {
        mObservers.addObserver(observer);
    }

    @Override
    public void removeObserver(TabModelObserver observer) {
        mObservers.removeObserver(observer);
    }

    @Override
    public void addIncognitoObserver(IncognitoTabModelObserver observer) {}

    @Override
    public void removeIncognitoObserver(IncognitoTabModelObserver observer) {}

    @Override
    public boolean isActiveModel() {
        return mIsActiveModel;
    }

    public void setAsActiveModelForTesting() {
        mIsActiveModel = true;
    }
}
