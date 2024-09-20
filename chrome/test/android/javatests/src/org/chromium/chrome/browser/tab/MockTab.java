// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import androidx.annotation.Nullable;

import com.google.common.collect.Lists;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

import java.util.List;

/** Tab used for various testing purposes. */
public class MockTab extends TabImpl {
    private GURL mGurlOverride;
    private WebContents mWebContentsOverride;
    // TODO(crbug.com/40187853) set mIsInitialized to true when initialize is called
    private boolean mIsInitialized;
    private boolean mIsDestroyed;
    private boolean mIsBeingRestored;

    private Boolean mCanGoBack;
    private Boolean mCanGoForward;

    private boolean mIsCustomTab;

    private Long mTimestampMillis;
    private Integer mParentId;

    /** Create a new Tab for testing and initializes Tab UserData objects. */
    public static MockTab createAndInitialize(int id, Profile profile) {
        MockTab tab = new MockTab(id, profile);
        tab.initialize(null, null, null, null, null, null, false, null, false);
        return tab;
    }

    /** Create a new Tab for testing and initializes Tab UserData objects. */
    public static MockTab createAndInitialize(
            int id, Profile profile, @TabLaunchType int tabLaunchType) {
        MockTab tab = new MockTab(id, profile, tabLaunchType);
        tab.initialize(null, null, null, null, null, null, false, null, false);
        return tab;
    }

    public MockTab(int id, Profile profile) {
        this(id, profile, TabLaunchType.UNSET);
    }

    public MockTab(int id, Profile profile, @TabLaunchType int tabLaunchType) {
        super(id, profile, tabLaunchType);
    }

    @Override
    public void initialize(
            Tab parent,
            @Nullable @TabCreationState Integer creationState,
            LoadUrlParams loadUrlParams,
            @Nullable String title,
            WebContents webContents,
            @Nullable TabDelegateFactory delegateFactory,
            boolean initiallyHidden,
            TabState tabState,
            boolean initializeRenderer) {
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

    /**
     * @param url The {@link GURL} to override with or null to remove the override.
     */
    public void setUrl(@Nullable GURL url) {
        mGurlOverride = url;
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

    @Override
    public void setParentId(int parentId) {
        mParentId = parentId;
    }

    /**
     * Overrides the {@link canGoBack} return value
     *
     * @param canGoBack The canGoBack return value or null to remove the override.
     */
    public void setCanGoBack(@Nullable Boolean canGoBack) {
        mCanGoBack = canGoBack;
    }

    @Override
    public boolean canGoBack() {
        if (mCanGoBack != null) {
            return mCanGoBack;
        }
        return super.canGoBack();
    }

    /**
     * Overrides the {@link canGoForward} return value
     *
     * @param canGoForward The canGoForward return value or null to remove the override.
     */
    public void setCanGoForward(@Nullable Boolean canGoForward) {
        mCanGoForward = canGoForward;
    }

    @Override
    public boolean canGoForward() {
        if (mCanGoForward != null) {
            return mCanGoForward;
        }
        return super.canGoForward();
    }

    @Override
    public void setTitle(String title) {
        super.setTitle(title);
    }

    public List<TabObserver> getObservers() {
        return Lists.newArrayList(mObservers);
    }
}
