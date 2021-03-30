// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;

/**
 * Tab used for various testing purposes.
 */
public class MockTab extends TabImpl {
    /**
     * Create a new Tab for testing and initializes Tab UserData objects.
     */
    public static Tab createAndInitialize(int id, boolean incognito) {
        TabImpl tab = new MockTab(id, incognito);
        tab.initialize(null, null, null, null, null, false, null);
        return tab;
    }

    /**
     * Create a new Tab for testing and initializes Tab UserData objects.
     */
    public static Tab createAndInitialize(
            int id, boolean incognito, @TabLaunchType int tabLaunchType) {
        TabImpl tab = new MockTab(id, incognito, tabLaunchType);
        tab.initialize(null, null, null, null, null, false, null);
        return tab;
    }

    public static TabImpl initializeWithCriticalPersistedTabData(
            TabImpl tab, CriticalPersistedTabData criticalPersistedTabData) {
        tab.getUserDataHost().setUserData(CriticalPersistedTabData.class, criticalPersistedTabData);
        tab.initialize(null, null, null, null, null, false, null);
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
            TabState tabState) {
        TabHelpers.initTabHelpers(this, parent);
    }

    public void broadcastOnLoadStopped(boolean toDifferentDocument) {
        for (TabObserver observer : mObservers) observer.onLoadStopped(this, toDifferentDocument);
    }
}
