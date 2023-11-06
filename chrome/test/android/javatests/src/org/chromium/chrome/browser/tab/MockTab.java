// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

/**
 * Tab used for various testing purposes.
 */
public class MockTab extends TabImpl {
    private GURL mGurlOverride;
    private WebContents mWebContentsOverride;
    // TODO(crbug.com/1223963) set mIsInitialized to true when initialize is called
    private boolean mIsInitialized;
    private boolean mIsDestroyed;
    private boolean mIsBeingRestored;

    private boolean mIsCustomTab;

    private Long mTimestampMillis;
    private Integer mParentId;

    /** Create a new Tab for testing and initializes Tab UserData objects. */
    public static MockTab createAndInitialize(int id, Profile profile) {
        MockTab tab = new MockTab(id, profile);
        tab.initialize(null, null, null, null, null, false, null, false);
        return tab;
    }

    /** Create a new Tab for testing and initializes Tab UserData objects. */
    public static MockTab createAndInitialize(
            int id, Profile profile, @TabLaunchType int tabLaunchType) {
        MockTab tab = new MockTab(id, profile, tabLaunchType);
        tab.initialize(null, null, null, null, null, false, null, false);
        return tab;
    }

    public MockTab(int id, Profile profile) {
        this(id, profile, null);
    }

    public MockTab(int id, Profile profile, @TabLaunchType Integer type) {
        super(id, profile, type);
    }

    @Override
    public void initialize(Tab parent, @Nullable @TabCreationState Integer creationState,
            LoadUrlParams loadUrlParams, WebContents webContents,
            @Nullable TabDelegateFactory delegateFactory, boolean initiallyHidden,
            TabState tabState, boolean initializeRenderer) {
        if (loadUrlParams != null) {
            mGurlOverride = new GURL(loadUrlParams.getUrl());
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

    public void setWebContentsOverrideForTesting(WebContents webContents) {
        mWebContentsOverride = webContents;
    }

    @Override
    public WebContents getWebContents() {
        if (mWebContentsOverride != null) {
            return mWebContentsOverride;
        }
        return super.getWebContents();
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

    @Override
    public long getTimestampMillis() {
        if (mTimestampMillis == null) {
            return super.getTimestampMillis();
        }
        return mTimestampMillis;
    }

    public void setTimestampMillis(long timestampMillis) {
        mTimestampMillis = timestampMillis;
    }

    @Override
    public int getParentId() {
        if (mParentId == null) {
            return super.getParentId();
        }
        return mParentId;
    }

    public void setParentId(int parentId) {
        mParentId = parentId;
    }

    @Override
    public void setTitle(String title) {
        super.setTitle(title);
    }
}
