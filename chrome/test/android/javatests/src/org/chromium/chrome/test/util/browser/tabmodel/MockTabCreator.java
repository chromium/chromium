// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser.tabmodel;

import android.util.SparseArray;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabTestUtils;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

/** MockTabCreator for use in tests. */
public class MockTabCreator extends TabCreator {
    public final SparseArray<TabState> created;
    public final CallbackHelper callback;

    private final boolean mIsIncognito;
    private final TabModelSelector mSelector;

    public int idOfFirstCreatedTab = Tab.INVALID_TAB_ID;

    public MockTabCreator(boolean incognito, TabModelSelector selector) {
        created = new SparseArray<>();
        callback = new CallbackHelper();
        mIsIncognito = incognito;
        mSelector = selector;
    }

    @Override
    public boolean createsTabsAsynchronously() {
        return false;
    }

    @Override
    public Tab createNewTab(LoadUrlParams loadUrlParams, @TabLaunchType int type, Tab parent) {
        Tab tab = new MockTab(0, mIsIncognito, TabLaunchType.FROM_LINK);
        mSelector.getModel(mIsIncognito)
                .addTab(tab, TabModel.INVALID_TAB_INDEX, type, TabCreationState.LIVE_IN_FOREGROUND);
        storeTabInfo(null, tab.getId());
        return tab;
    }

    @Override
    public Tab createFrozenTab(TabState state, byte[] criticalPersistedTabData, int id,
            boolean isIncognito, int index) {
        Tab tab = new MockTab(id, state.isIncognito(), TabLaunchType.FROM_RESTORE);
        TabTestUtils.restoreFieldsFromState(tab, state);
        mSelector.getModel(mIsIncognito)
                .addTab(tab, index, TabLaunchType.FROM_RESTORE, TabCreationState.FROZEN_ON_RESTORE);
        storeTabInfo(state, id);
        return tab;
    }

    @Override
    public boolean createTabWithWebContents(
            Tab parent, WebContents webContents, @TabLaunchType int type, GURL url) {
        return false;
    }

    @Override
    public Tab launchUrl(String url, @TabLaunchType int type) {
        return null;
    }

    private void storeTabInfo(TabState state, int id) {
        if (created.size() == 0) idOfFirstCreatedTab = id;
        created.put(id, state);
        callback.notifyCalled();
    }
}