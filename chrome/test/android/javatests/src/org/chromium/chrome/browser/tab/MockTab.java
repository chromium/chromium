// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

/**
 * Tab used for various testing purposes.
 */
public class MockTab extends TabImpl {
    private GURL mGurlOverride;
    // TODO(crbug.com/1223963) set mIsInitialized to true when initialize is called
    private boolean mIsInitialized;
    private boolean mIsDestroyed;
    private boolean mIsBeingRestored;

    private boolean mIsCustomTab;

    /**
     * Create a new Tab for testing and initializes Tab UserData objects.
     */
    public static Tab createAndInitialize(int id, boolean incognito) {
        TabImpl tab = new MockTab(id, incognito);
        tab.initialize(null, null, null, null, null, false, null, false);
        return tab;
    }

    /**
     * Create a new Tab for testing and initializes Tab UserData objects.
     */
    public static Tab createAndInitialize(
            int id, boolean incognito, @TabLaunchType int tabLaunchType) {
        TabImpl tab = new MockTab(id, incognito, tabLaunchType);
        tab.initialize(null, null, null, null, null, false, null, false);
        return tab;
    }

    public static TabImpl initializeWithCriticalPersistedTabData(
            TabImpl tab, CriticalPersistedTabData criticalPersistedTabData) {
        tab.getUserDataHost().setUserData(CriticalPersistedTabData.class, criticalPersistedTabData);
        tab.initialize(null, null, null, null, null, false, null, false);
        return tab;
    }

    /**
     * Constructor for id and incognito attribute. Tests often need to initialize
     * these two fields only.
     */
    public MockTab(int id, boolean incognito) {
        super(id, incognito, null, null);
    }

    public MockTab(int id, boolean incognito, @TabLaunchType Integer type) {
        super(id, incognito, type, null);
    }

    @Override
    public void initialize(Tab parent, @Nullable @TabCreationState Integer creationState,
            LoadUrlParams loadUrlParams, WebContents webContents,
            @Nullable TabDelegateFactory delegateFactory, boolean initiallyHidden,
            TabState tabState, boolean initializeRenderer) {
        if (loadUrlParams != null) {
            mGurlOverride = new GURL(loadUrlParams.getUrl());
            CriticalPersistedTabData.from(this).setUrl(mGurlOverride);
        }
        TabHelpers.initTabHelpers(this, parent);
    }

    @Override
    public GURL getUrl() {
        if (mGurlOverride == null) {
            return super.getUrl();
        }
        return mGurlOverride;
    }

    public void broadcastOnLoadStopped(boolean toDifferentDocument) {
        for (TabObserver observer : mObservers) observer.onLoadStopped(this, toDifferentDocument);
    }

    public void setGurlOverrideForTesting(GURL url) {
        mGurlOverride = url;
    }

    @Override
    public boolean isInitialized() {
        return mIsInitialized;
    }

    @Override
    public boolean isDestroyed() {
        return mIsDestroyed;
    }

    public void setIsInitialized(boolean isInitialized) {
        mIsInitialized = isInitialized;
    }

    public void setIsCustomTab(boolean isCustomTab) {
        mIsCustomTab = isCustomTab;
    }

    @Override
    public void destroy() {
        mIsDestroyed = true;
        mIsInitialized = false;
        for (TabObserver observer : mObservers) observer.onDestroyed(this);
        mObservers.clear();
    }

    @Override
    public boolean isCustomTab() {
        return mIsCustomTab;
    }

    @Override
    public boolean isBeingRestored() {
        return mIsBeingRestored;
    }

    public void setIsBeingRestored(boolean isBeingRestored) {
        mIsBeingRestored = isBeingRestored;
    }
}
